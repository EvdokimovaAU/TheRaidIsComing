
#include "AI/EnemyAI.h"
#include "UI/RICOverheadHealthBar.h"
#include "AI/EnemyBarrier.h"

#include "AI/GameBalanceManager.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyMageAI.h"
#include "AI/EnemyTorchbearerAI.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Construction/RICMoneyComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"

// CONSTRUCTOR

AEnemyAI::AEnemyAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = AAIController::StaticClass();

    AutoPossessAI =
        EAutoPossessAI::PlacedInWorldOrSpawned;

    GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}

// BEGIN PLAY

void AEnemyAI::BeginPlay()
{
    Super::BeginPlay();

    bKillRewardGranted = false;

    // Initialize health from the level-specific maximum.
    ApplyEnemyBalance();
    CurrentHP = MaxHP;
    const FVector Anchor = GetActorLocation() +
        FVector::UpVector * (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 15.f);
    URICOverheadHealthBar::Attach(this, Anchor, FIntPoint(80, 11),
        TAttribute<FVector2D>::CreateWeakLambda(this, [this]()
        {
            return FVector2D(CurrentHP, MaxHP);
        }), false);

    if (auto* Mage = Cast<AEnemyMageAI>(this))
    {
        URICOverheadHealthBar::Attach(this, Anchor, FIntPoint(80, 11),
            TAttribute<FVector2D>::CreateWeakLambda(Mage, [Mage]()
            {
                if (Mage->HasActiveBarrier())
                {
                    return FVector2D(Mage->ActiveBarrier->CurrentBarrierHP, Mage->BarrierHP);
                }
                return FVector2D::ZeroVector;
            }), false, true);
    }

    GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;

    if (IsValid(Capitol))
    {
        SetCapitol(Capitol.Get());
    }

    if (bAutoStartMovement)
    {
        GetWorldTimerManager().SetTimer(
            MovementStartTimer,
            this,
            &AEnemyAI::TryStartMovement,
            0.2f,
            true
        );

        TryStartMovement();
    }
}

// APPLY GAME BALANCE

void AEnemyAI::ApplyEnemyBalance()
{
    AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this);

    if (!IsValid(Balance))
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("[EnemyAI] %s: BalanceManager not found. Using BP settings."),
            *GetName());

        return;
    }

    const FEnemyBalanceSettings* Settings = nullptr;

    // Check specific enemy classes.
    if (IsA(AEnemySwordsmanAI::StaticClass()))
    {
        Settings = &Balance->Swordsman;
    }
    else if (IsA(AEnemyMageAI::StaticClass()))
    {
        Settings = &Balance->Mage;
    }
    else if (IsA(AEnemyTorchbearerAI::StaticClass()))
    {
        Settings = &Balance->Torchbearer;
    }

    if (!Settings)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("[EnemyAI] %s: No balance profile for this enemy type."),
            *GetName());

        return;
    }

    ApplyCombatBalance(
        Settings->MaxHealth,
        Settings->AttackDamage,
        Settings->AttackRange,
        Settings->AttackCooldown
    );

    MovementSpeed =
        FMath::Max(0.0f, Settings->MovementSpeed);

    KillReward =
        FMath::Max(0, Settings->KillReward);

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = MovementSpeed;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[EnemyAI] %s balance: HP=%.1f Damage=%.1f Speed=%.1f Reward=%d"),
        *GetName(),
        MaxHP,
        Damage,
        MovementSpeed,
        KillReward);
}

// WAIT FOR AI INITIALIZATION

void AEnemyAI::TryStartMovement()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(
            MovementStartTimer
        );

        return;
    }

    if (!IsValid(CurrentTarget))
    {
        return;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (!EnemyController)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    MoveToTarget();
}

// ASSIGN CAPITOL

void AEnemyAI::SetCapitol(AActor* NewCapitol)
{
    if (!IsValid(NewCapitol) || NewCapitol == this)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Invalid Capitol"));

        return;
    }

    Capitol = NewCapitol;
    CurrentTarget = NewCapitol;

    UE_LOG(LogTemp, Log,
        TEXT("[EnemyAI] %s: Capitol assigned: %s"),
        *GetName(),
        *NewCapitol->GetName());

    if (bAutoStartMovement && HasActorBegunPlay())
    {
        TryStartMovement();
    }
}

// GET CURRENT TARGET

AActor* AEnemyAI::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

// NAVIGATION RETRY

void AEnemyAI::ScheduleNavigationRetry()
{
    if (!IsAlive() ||
        !IsValid(CurrentTarget) ||
        !GetWorld())
    {
        return;
    }

    if (!GetWorldTimerManager().IsTimerActive(
        NavigationRetryTimer))
    {
        GetWorldTimerManager().SetTimer(
            NavigationRetryTimer,
            this,
            &AEnemyAI::MoveToTarget,
            0.5f,
            true
        );
    }
}

// MOVE TO TARGET

FVector AEnemyAI::CapitolRouteGoal(const FVector& Goal, TArray<FVector>* Route)
{
    if (!GetWorld()->GetName().EndsWith(TEXT("Level_2"))) return Goal;
    const FVector Start = GetActorLocation();
    // Each island on Level_2 has a northern and a southern passage.
    const double Direction = Start.X >= 0 ? 1.0 : -1.0;
    const double IslandX = Direction * 1400.0;
    if (Start.X * Direction <= 1580.0 || Goal.X * Direction >= 1400.0) return Goal;

    if (!LevelTwoLaneSign) LevelTwoLaneSign = FMath::RandBool() ? 1 : -1;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Lane;
    if (!Nav || !Nav->ProjectPointToNavigation(FVector(IslandX, LevelTwoLaneSign * 650.0, Start.Z),
        Lane, FVector(80, 80, 250))) return Goal;
    auto* First = Nav->FindPathToLocationSynchronously(GetWorld(), Start, Lane.Location, this);
    auto* Last = Nav->FindPathToLocationSynchronously(GetWorld(), Lane.Location, Goal, this);
    if (!IsValid(First) || !First->IsValid() || First->IsPartial() || First->PathPoints.Num() < 2 ||
        !IsValid(Last) || !Last->IsValid() || Last->IsPartial() || Last->PathPoints.Num() < 2) return Goal;
    if (Route)
    {
        *Route = First->PathPoints;
        Route->Append(Last->PathPoints.GetData() + 1, Last->PathPoints.Num() - 1);
    }
    return Lane.Location;
}

void AEnemyAI::MoveToTarget()
{
    if (!IsAlive() || !IsValid(CurrentTarget))
    {
        return;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (!EnemyController)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] AIController not found"));

        ScheduleNavigationRetry();
        return;
    }

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] NavigationSystem not found"));

        ScheduleNavigationRetry();
        return;
    }

    const FVector EnemyLocation =
        GetActorLocation();

    const FVector TargetLocation =
        CurrentTarget->GetActorLocation();

    // PROJECT ENEMY ON NAVMESH

    FNavLocation EnemyNavLocation;

    const bool bEnemyOnNavMesh =
        NavSys->ProjectPointToNavigation(
            EnemyLocation,
            EnemyNavLocation,
            FVector(150.f, 150.f, 250.f)
        );

    if (!bEnemyOnNavMesh)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] Enemy is outside NavMesh"));

        ScheduleNavigationRetry();
        return;
    }

    // FIND NAVIGATION POINT NEAR TARGET

    FNavLocation TargetNavLocation;

    const bool bTargetOnNavMesh =
        NavSys->ProjectPointToNavigation(
            TargetLocation,
            TargetNavLocation,
            FVector(
                NavigationSearchRadius,
                NavigationSearchRadius,
                300.f
            )
        );

    if (!bTargetOnNavMesh)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] No NavMesh near target: %s"),
            *CurrentTarget->GetName());

        ScheduleNavigationRetry();
        return;
    }

    // CHECK PATH

    UNavigationPath* Path =
        NavSys->FindPathToLocationSynchronously(
            GetWorld(),
            EnemyNavLocation.Location,
            TargetNavLocation.Location,
            this
        );

    if (!IsValid(Path) ||
        !Path->IsValid() ||
        Path->IsPartial())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Complete path not found"));

        ScheduleNavigationRetry();
        return;
    }

    GetWorldTimerManager().ClearTimer(
        NavigationRetryTimer
    );

    // DIAGNOSTICS

    UE_LOG(LogTemp, Warning,
        TEXT("========== ENEMY MOVEMENT =========="));

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy: %s"),
        *GetName());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Target: %s"),
        *CurrentTarget->GetName());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Destination: %s"),
        *TargetNavLocation.Location.ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Movement speed: %.2f"),
        GetCharacterMovement()->MaxWalkSpeed);

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy position: %s"),
        *GetActorLocation().ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Capitol position: %s"),
        *CurrentTarget->GetActorLocation().ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Navigation destination: %s"),
        *TargetNavLocation.Location.ToString());

    // START MOVEMENT

    const FVector Goal = CurrentTarget == Capitol ? CapitolRouteGoal(TargetNavLocation.Location) : TargetNavLocation.Location;

    const EPathFollowingRequestResult::Type Result =
        EnemyController->MoveToLocation(
            Goal,
            AcceptanceRadius,
            true,
            true,
            false,
            true,
            nullptr,
            true
        );

    if (!Goal.Equals(TargetNavLocation.Location)) ScheduleNavigationRetry();
    switch (Result)
    {
    case EPathFollowingRequestResult::RequestSuccessful:
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Movement started"));
        break;

    case EPathFollowingRequestResult::AlreadyAtGoal:
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Already at target"));
        break;

    case EPathFollowingRequestResult::Failed:
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] Movement FAILED"));

        ScheduleNavigationRetry();
        break;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("===================================="));
}

// STOP MOVEMENT

void AEnemyAI::StopMovement()
{
    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (EnemyController)
    {
        EnemyController->StopMovement();
    }
}

// OBSTACLE DETECTION

void AEnemyAI::OnObstacleDetected(AActor* Obstacle)
{
    // Implemented by derived enemy classes.
}

// GRANT KILL REWARD

void AEnemyAI::GrantKillReward()
{
    if (bKillRewardGranted)
    {
        return;
    }

    bKillRewardGranted = true;

    if (KillReward <= 0)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!IsValid(PlayerController))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Kill reward failed: PlayerController not found"));

        return;
    }

    URICMoneyComponent* MoneyComponent =
        PlayerController->FindComponentByClass<URICMoneyComponent>();

    if (!IsValid(MoneyComponent))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Kill reward failed: MoneyComponent not found"));

        return;
    }

    const int32 AddedMoney =
        MoneyComponent->AddMoney(KillReward);

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy killed: %s | Reward: %d | Added: %d | Balance: %d"),
        *GetName(),
        KillReward,
        AddedMoney,
        MoneyComponent->CurrentMoney);
}

// DEATH

void AEnemyAI::Die()
{
    GetWorldTimerManager().ClearTimer(
        NavigationRetryTimer
    );

    if (bIsDead)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    StopMovement();

    Super::Die();

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy died: %s"),
        *GetName());

    GrantKillReward();

    OnEnemyDied.Broadcast(this);

    if (UCapsuleComponent* Capsule =
        GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    if (USkeletalMeshComponent* EnemyMesh =
        GetMesh())
    {
        EnemyMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    const float ActualDestroyDelay =
        FMath::Max(DeathDestroyDelay, 0.01f);

    SetLifeSpan(ActualDestroyDelay);

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] %s scheduled for removal in %.2f seconds"),
        *GetName(),
        ActualDestroyDelay);
}

// END PLAY

void AEnemyAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        NavigationRetryTimer
    );

    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    Super::EndPlay(EndPlayReason);
}
