
#include "AI/EnemyMageAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemySwordsmanAI.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = false;

    bAutoStartMovement = false;

    MovementSpeed = 300.0f;
    AcceptanceRadius = 150.0f;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemyMageAI::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = MageMovementSpeed;
        Movement->MaxAcceleration = MageAcceleration;
        Movement->BrakingDecelerationWalking =
            MageBrakingDeceleration;

        Movement->GroundFriction = 4.0f;
        Movement->bRequestedMoveUseAcceleration = true;
    }

    CurrentDesiredSpeed = MageMovementSpeed;

    GetWorldTimerManager().SetTimer(
        BehaviorTimer,
        this,
        &AEnemyMageAI::UpdateBehavior,
        0.1f,
        true
    );

    UpdateBehavior();
}

// ==========================================
// SEARCH SWORDSMEN
// ==========================================

bool AEnemyMageAI::IsSwordsman(
    const AEnemyAI* Enemy) const
{
    return IsValid(Enemy) &&
        Enemy->IsA(AEnemySwordsmanAI::StaticClass());
}

void AEnemyMageAI::FindAlliesAhead()
{
    AlliesAhead.Reset();

    if (!GetWorld())
        return;

    FVector Forward = GetActorForwardVector();

    if (IsValid(Capitol))
    {
        Forward =
            Capitol->GetActorLocation() -
            GetActorLocation();
    }

    Forward.Z = 0.0f;

    if (!Forward.Normalize())
        return;

    for (TActorIterator<AEnemySwordsmanAI>
        It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Ally = *It;

        if (!IsValid(Ally) || !Ally->IsAlive())
            continue;

        FVector Offset =
            Ally->GetActorLocation() -
            GetActorLocation();

        Offset.Z = 0.0f;

        if (Offset.SizeSquared() >
            FMath::Square(AllyDetectionRadius))
        {
            continue;
        }

        if (Offset.IsNearlyZero())
            continue;

        if (FVector::DotProduct(
            Forward,
            Offset.GetSafeNormal()) > 0.0f)
        {
            AlliesAhead.Add(Ally);
        }
    }
}

// ==========================================
// SELECT SWORDSMAN
// ==========================================

void AEnemyMageAI::SelectSwordsman()
{
    FollowedSwordsman.Reset();

    float BestDistance =
        TNumericLimits<float>::Max();

    for (AEnemyAI* Ally : AlliesAhead)
    {
        if (!IsSwordsman(Ally) || !Ally->IsAlive())
            continue;

        const float Distance =
            FVector::DistSquared2D(
                GetActorLocation(),
                Ally->GetActorLocation()
            );

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            FollowedSwordsman = Ally;
        }
    }
}

// ==========================================
// BARRIER
// ==========================================

bool AEnemyMageAI::HasActiveBarrier() const
{
    return IsValid(ActiveBarrier) &&
        ActiveBarrier->IsBarrierActive();
}

bool AEnemyMageAI::CanCreateBarrier() const
{
    return IsAlive() &&
        !HasActiveBarrier() &&
        BarrierClass != nullptr &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= NextBarrierTime;
}

void AEnemyMageAI::Barrier()
{
    if (!CanCreateBarrier())
        return;

    MageState = EMageState::CreatingBarrier;

    // Создание барьера не останавливает мага.

    FActorSpawnParameters Params;
    Params.Owner = this;

    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveBarrier =
        GetWorld()->SpawnActor<AEnemyBarrier>(
            BarrierClass,
            GetActorLocation(),
            FRotator::ZeroRotator,
            Params
        );

    if (!IsValid(ActiveBarrier))
    {
        NextBarrierTime =
            GetWorld()->GetTimeSeconds() +
            BarrierCooldown;

        UE_LOG(LogTemp, Warning,
            TEXT("[Mage] Failed to create barrier"));

        return;
    }

    ActiveBarrier->ActivateBarrier(
        BarrierHP,
        BarrierDuration
    );

    ActiveBarrier->AttachToMage(this);

    NextBarrierTime =
        GetWorld()->GetTimeSeconds() +
        BarrierCooldown;

    UE_LOG(LogTemp, Warning,
        TEXT("[Mage] Barrier created: %s"),
        *GetName());
}

void AEnemyMageAI::RemoveBarrier()
{
    if (IsValid(ActiveBarrier))
    {
        ActiveBarrier->RemoveBarrier();
        ActiveBarrier->Destroy();
    }

    ActiveBarrier = nullptr;
}

// ==========================================
// FOLLOW SPEED
// ==========================================

void AEnemyMageAI::UpdateFollowSpeed(
    float DeltaTime)
{
    UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    if (!Movement)
        return;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    float TargetSpeed = MageMovementSpeed;

    if (IsValid(Ally) && Ally->IsAlive())
    {
        const float Distance =
            FVector::Dist2D(
                GetActorLocation(),
                Ally->GetActorLocation());

        const float AllySpeed =
            Ally->GetVelocity().Size2D();

        if (Distance >= ResumeFollowDistance)
        {
            // Маг отстал и ускоряется.
            TargetSpeed = MageMovementSpeed;
        }
        else
        {
            // Подстраивается под скорость мечника.
            TargetSpeed = FMath::Clamp(
                AllySpeed,
                MinFollowSpeed,
                MageMovementSpeed
            );
        }
    }

    CurrentDesiredSpeed = FMath::FInterpConstantTo(
        CurrentDesiredSpeed,
        TargetSpeed,
        DeltaTime,
        SpeedChangeRate
    );

    Movement->MaxWalkSpeed = CurrentDesiredSpeed;
}

// ==========================================
// MOVEMENT
// ==========================================

void AEnemyMageAI::UpdateMovement()
{
    if (!IsAlive())
        return;

    AAIController* MageController =
        Cast<AAIController>(GetController());

    if (!MageController)
        return;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    if (IsValid(Ally) && Ally->IsAlive())
    {
        MageState = EMageState::FollowingSwordsmen;

        const bool bTargetChanged =
            CurrentMoveTarget.Get() != Ally ||
            bMovingToCapitol;

        if (bTargetChanged)
        {
            CurrentMoveTarget = Ally;
            bMovingToCapitol = false;
            bFollowingMoveActive = false;
        }

        const float Distance =
            FVector::Dist2D(
                GetActorLocation(),
                Ally->GetActorLocation());

        if (!bFollowingMoveActive &&
            Distance >= ResumeFollowDistance)
        {
            MageController->MoveToActor(
                Ally,
                FollowDistance,
                true,
                true,
                true
            );

            // Проверяем, запущено ли движение.
            bFollowingMoveActive =
                MageController->GetMoveStatus() ==
                EPathFollowingStatus::Moving;

            UE_LOG(LogTemp, Log,
                TEXT("[Mage] Following %s | Distance: %.1f"),
                *Ally->GetName(),
                Distance);
        }

        // Не прерываем активный путь.
        // Движение завершается автоматически
        // при достижении FollowDistance.

        return;
    }

    // ======================================
    // NO SWORDSMAN - MOVE TO CAPITOL
    // ======================================

    bFollowingMoveActive = false;

    if (!IsValid(Capitol))
        return;

    MageState = HasActiveBarrier()
        ? EMageState::MovingAlone
        : EMageState::WaitingForBarrier;

    if (!bMovingToCapitol ||
        CurrentMoveTarget.Get() != Capitol.Get())
    {
        CurrentMoveTarget = Capitol.Get();
        bMovingToCapitol = true;

        Super::MoveToTarget();

        UE_LOG(LogTemp, Log,
            TEXT("[Mage] Moving to Capitol"));
    }
}

void AEnemyMageAI::MoveToTarget()
{
    UpdateMovement();
}

// ==========================================
// BEHAVIOR
// ==========================================

void AEnemyMageAI::UpdateBehavior()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(
            BehaviorTimer);

        return;
    }

    FindAlliesAhead();

    if (!FollowedSwordsman.IsValid() ||
        !FollowedSwordsman->IsAlive())
    {
        SelectSwordsman();
    }

    if (IsValid(ActiveBarrier) &&
        !ActiveBarrier->IsBarrierActive())
    {
        RemoveBarrier();
        MageState = EMageState::WaitingForBarrier;
    }

    if (!HasActiveBarrier() &&
        CanCreateBarrier())
    {
        Barrier();
    }

    UpdateMovement();
    UpdateFollowSpeed(0.1f);
}

// ==========================================
// DEATH
// ==========================================

void AEnemyMageAI::Die()
{
    if (!IsAlive())
        return;

    GetWorldTimerManager().ClearTimer(
        BehaviorTimer);

    RemoveBarrier();

    Super::Die();
}

void AEnemyMageAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        BehaviorTimer);

    RemoveBarrier();

    Super::EndPlay(EndPlayReason);
}
