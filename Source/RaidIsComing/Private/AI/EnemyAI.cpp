
#include "AI/EnemyAI.h"

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

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemyAI::AEnemyAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = AAIController::StaticClass();

    AutoPossessAI =
        EAutoPossessAI::PlacedInWorldOrSpawned;

    GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemyAI::BeginPlay()
{
    Super::BeginPlay();

    bKillRewardGranted = false;

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

// ==========================================
// WAIT FOR AI INITIALIZATION
// ==========================================

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

// ==========================================
// ASSIGN CAPITOL
// ==========================================

void AEnemyAI::SetCapitol(AActor* NewCapitol)
{
    if (!IsValid(NewCapitol) || NewCapitol == this)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Invalid Capitol")
        );

        return;
    }

    Capitol = NewCapitol;
    CurrentTarget = NewCapitol;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[EnemyAI] %s: Capitol assigned: %s"),
        *GetName(),
        *NewCapitol->GetName()
    );

    if (bAutoStartMovement && HasActorBegunPlay())
    {
        TryStartMovement();
    }
}

// ==========================================
// GET CURRENT TARGET
// ==========================================

AActor* AEnemyAI::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

// ==========================================
// MOVE TO TARGET
// ==========================================

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemyAI] AIController not found")
        );

        return;
    }

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemyAI] NavigationSystem not found")
        );

        return;
    }

    const FVector EnemyLocation =
        GetActorLocation();

    const FVector TargetLocation =
        CurrentTarget->GetActorLocation();

    // ======================================
    // PROJECT ENEMY ON NAVMESH
    // ======================================

    FNavLocation EnemyNavLocation;

    const bool bEnemyOnNavMesh =
        NavSys->ProjectPointToNavigation(
            EnemyLocation,
            EnemyNavLocation,
            FVector(150.f, 150.f, 250.f)
        );

    if (!bEnemyOnNavMesh)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemyAI] Enemy is outside NavMesh")
        );

        return;
    }

    // ======================================
    // FIND NAVIGATION POINT NEAR TARGET
    // ======================================

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemyAI] No NavMesh near target: %s"),
            *CurrentTarget->GetName()
        );

        return;
    }

    // ======================================
    // CHECK PATH
    // ======================================

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
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Complete path not found")
        );

        return;
    }

    // ======================================
    // DIAGNOSTICS
    // ======================================

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("========== ENEMY MOVEMENT ==========")
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Enemy: %s"),
        *GetName()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Target: %s"),
        *CurrentTarget->GetName()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Destination: %s"),
        *TargetNavLocation.Location.ToString()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Movement speed: %.2f"),
        GetCharacterMovement()->MaxWalkSpeed
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Enemy position: %s"),
        *GetActorLocation().ToString()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Capitol position: %s"),
        *CurrentTarget->GetActorLocation().ToString()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Navigation destination: %s"),
        *TargetNavLocation.Location.ToString()
    );

    // ======================================
    // START MOVEMENT
    // ======================================

    const EPathFollowingRequestResult::Type Result =
        EnemyController->MoveToLocation(
            TargetNavLocation.Location,
            AcceptanceRadius,
            true,
            true,
            false,
            true,
            nullptr,
            true
        );

    switch (Result)
    {
    case EPathFollowingRequestResult::RequestSuccessful:

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Movement started")
        );

        break;

    case EPathFollowingRequestResult::AlreadyAtGoal:

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Already at target")
        );

        break;

    case EPathFollowingRequestResult::Failed:

        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemyAI] Movement FAILED")
        );

        break;
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("====================================")
    );
}

// ==========================================
// STOP MOVEMENT
// ==========================================

void AEnemyAI::StopMovement()
{
    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (EnemyController)
    {
        EnemyController->StopMovement();
    }
}

// ==========================================
// OBSTACLE DETECTION
// ==========================================

void AEnemyAI::OnObstacleDetected(AActor* Obstacle)
{
    // Implemented by derived enemy classes.
}

// ==========================================
// GRANT KILL REWARD
// ==========================================

void AEnemyAI::GrantKillReward()
{
    // Reward must only be granted once.
    if (bKillRewardGranted)
    {
        return;
    }

    bKillRewardGranted = true;

    // Zero reward is allowed.
    if (KillReward <= 0)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    // The money component belongs to the
    // player's controller, not the enemy.
    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!IsValid(PlayerController))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Kill reward failed: PlayerController not found")
        );

        return;
    }

    URICMoneyComponent* MoneyComponent =
        PlayerController->FindComponentByClass<URICMoneyComponent>();

    if (!IsValid(MoneyComponent))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAI] Kill reward failed: MoneyComponent not found")
        );

        return;
    }

    const int32 AddedMoney =
        MoneyComponent->AddMoney(KillReward);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Enemy killed: %s | Reward: %d | Added: %d | Balance: %d"),
        *GetName(),
        KillReward,
        AddedMoney,
        MoneyComponent->CurrentMoney
    );
}

// ==========================================
// DEATH
// ==========================================

void AEnemyAI::Die()
{
    // ReceiveDamage() reduces HP to zero
    // before calling Die().
    if (bIsDead)
    {
        return;
    }

    // Stop delayed movement initialization.
    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    // Stop active AI movement.
    StopMovement();

    // Mark enemy as dead.
    Super::Die();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] Enemy died: %s"),
        *GetName()
    );

    // Award money once for this death.
    GrantKillReward();

    // Notify spawner and WaveManager.
    OnEnemyDied.Broadcast(this);

    // Dead enemy no longer blocks movement.
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    if (USkeletalMeshComponent* EnemyMesh = GetMesh())
    {
        EnemyMesh->SetCollisionEnabled(
            ECollisionEnabled::NoCollision
        );
    }

    // Remove dead enemy after delay.
    const float ActualDestroyDelay =
        FMath::Max(DeathDestroyDelay, 0.01f);

    SetLifeSpan(ActualDestroyDelay);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemyAI] %s scheduled for removal in %.2f seconds"),
        *GetName(),
        ActualDestroyDelay
    );
}

// ==========================================
// END PLAY
// ==========================================

void AEnemyAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    Super::EndPlay(EndPlayReason);
}
