
#include "AI/EnemyMageAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemySwordsmanAI.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = false;
    bAutoStartMovement = false;
    MovementSpeed = 250.f;
    AcceptanceRadius = 100.f;
}

void AEnemyMageAI::BeginPlay()
{
    Super::BeginPlay();

    if (auto* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = NormalMovementSpeed;
        Movement->MaxAcceleration = MageAcceleration;
        Movement->BrakingDecelerationWalking =
            MageBrakingDeceleration;
        Movement->GroundFriction = 4.f;
        Movement->bRequestedMoveUseAcceleration = true;
    }

    CurrentDesiredSpeed = NormalMovementSpeed;

    GetWorldTimerManager().SetTimer(
        BehaviorTimer,
        this,
        &AEnemyMageAI::UpdateBehavior,
        FMath::Max(0.05f, FollowUpdateInterval),
        true
    );

    UpdateBehavior();
}

// ==========================================
// ALLY SEARCH
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

    for (TActorIterator<AEnemySwordsmanAI>
        It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Ally = *It;

        if (!IsValid(Ally) || !Ally->IsAlive())
            continue;

        if (FVector::DistSquared2D(
            GetActorLocation(),
            Ally->GetActorLocation()) <=
            FMath::Square(AllyDetectionRadius))
        {
            AlliesAhead.Add(Ally);
        }
    }
}

void AEnemyMageAI::SelectSwordsman()
{
    AEnemyAI* BestAlly = nullptr;
    float BestDistance = TNumericLimits<float>::Max();

    for (AEnemyAI* Ally : AlliesAhead)
    {
        if (!IsSwordsman(Ally) || !Ally->IsAlive())
            continue;

        const float Distance = FVector::DistSquared2D(
            GetActorLocation(),
            Ally->GetActorLocation()
        );

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestAlly = Ally;
        }
    }

    if (FollowedSwordsman.Get() != BestAlly)
    {
        FollowedSwordsman = BestAlly;
        bFollowingSwordsman = false;
        bMovingToCapitol = false;
    }
}

// ==========================================
// GROUP WAITING
// ==========================================

void AEnemyMageAI::StopNearbySwordsmen()
{
    if (!GetWorld())
        return;

    WaitingSwordsmen.Empty();

    for (TActorIterator<AEnemySwordsmanAI>
        It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Swordsman = *It;

        if (!IsValid(Swordsman) ||
            !Swordsman->IsAlive())
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Swordsman->GetActorLocation()
        );

        if (Distance > AllyDetectionRadius)
            continue;

        if (Swordsman->IsAttackingWall() ||
            Swordsman->IsAttackingCapitol() ||
            Swordsman->IsWaitingForMage())
        {
            continue;
        }

        Swordsman->WaitForMage(this);

        if (Swordsman->IsWaitingForMage())
        {
            WaitingSwordsmen.Add(Swordsman);
        }
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[Mage] Swordsmen waiting: %d"),
        WaitingSwordsmen.Num()
    );
}

void AEnemyMageAI::ReleaseWaitingSwordsmen()
{
    for (const TWeakObjectPtr<AEnemySwordsmanAI>& WeakSwordsman
        : WaitingSwordsmen)
    {
        AEnemySwordsmanAI* Swordsman =
            WeakSwordsman.Get();

        if (IsValid(Swordsman))
        {
            Swordsman->ResumeAfterMage(this);
        }
    }

    WaitingSwordsmen.Empty();
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
        !bCastingBarrier &&
        !bAttacking &&
        !HasActiveBarrier() &&
        BarrierClass != nullptr &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= NextBarrierTime;
}

void AEnemyMageAI::Barrier()
{
    if (!CanCreateBarrier())
        return;

    bCastingBarrier = true;
    bBarrierSpawnedThisCast = false;
    MageState = EMageState::CreatingBarrier;

    // Stop mage before casting.
    StopMovement();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    bFollowingSwordsman = false;
    bMovingToCapitol = false;
    CurrentMoveTarget.Reset();

    // Stop nearby swordsmen for the cast duration.
    StopNearbySwordsmen();

    float Duration = 0.0f;

    if (BarrierMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            Duration = Anim->Montage_Play(BarrierMontage);
        }
    }

    if (Duration <= 0.0f)
    {
        FinishBarrierCast();
        return;
    }

    GetWorldTimerManager().SetTimer(
        BarrierCastEndTimer,
        this,
        &AEnemyMageAI::FinishBarrierCast,
        Duration,
        false
    );
}

void AEnemyMageAI::SpawnBarrierFromAnimation()
{
    if (!bCastingBarrier ||
        bBarrierSpawnedThisCast ||
        !IsAlive())
    {
        return;
    }

    bBarrierSpawnedThisCast = true;

    GetWorldTimerManager().ClearTimer(BarrierSpawnTimer);

    SpawnBarrierInternal();
}

void AEnemyMageAI::SpawnBarrierInternal()
{
    if (!GetWorld() ||
        !BarrierClass ||
        HasActiveBarrier())
    {
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveBarrier = GetWorld()->SpawnActor<AEnemyBarrier>(
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

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Mage] Barrier spawn failed")
        );

        return;
    }

    ActiveBarrier->ActivateBarrier(
        BarrierHP,
        BarrierDuration
    );

    ActiveBarrier->AttachToMage(this);

    bBarrierDestroyed = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Mage] Barrier activated | HP: %.1f"),
        BarrierHP
    );
}

void AEnemyMageAI::FinishBarrierCast()
{
    if (!bCastingBarrier)
        return;

    GetWorldTimerManager().ClearTimer(BarrierSpawnTimer);
    GetWorldTimerManager().ClearTimer(BarrierCastEndTimer);

    // Create the barrier after the animation.
    if (!bBarrierSpawnedThisCast && IsAlive())
    {
        SpawnBarrierFromAnimation();
    }

    bCastingBarrier = false;

    MageState = FollowedSwordsman.IsValid()
        ? EMageState::FollowingSwordsmen
        : EMageState::MovingAlone;

    bFollowingSwordsman = false;
    bMovingToCapitol = false;
    CurrentMoveTarget.Reset();

    // Release the group.
    ReleaseWaitingSwordsmen();

    UpdateMovement();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[Mage] Barrier cast finished. Group moving.")
    );
}

void AEnemyMageAI::CancelBarrierCast()
{
    GetWorldTimerManager().ClearTimer(BarrierSpawnTimer);
    GetWorldTimerManager().ClearTimer(BarrierCastEndTimer);

    bCastingBarrier = false;
    bBarrierSpawnedThisCast = false;

    if (BarrierMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(0.1f, BarrierMontage);
        }
    }

    ReleaseWaitingSwordsmen();
}

void AEnemyMageAI::RemoveBarrierInternal(bool bDestroyed)
{
    if (IsValid(ActiveBarrier))
    {
        ActiveBarrier->RemoveBarrier();
        ActiveBarrier->Destroy();
    }

    ActiveBarrier = nullptr;
    bBarrierDestroyed = bDestroyed;

    if (bDestroyed && GetWorld())
    {
        NextBarrierTime =
            GetWorld()->GetTimeSeconds() +
            FMath::Max(0.f, BarrierCooldown);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Mage] Barrier destroyed. Cooldown started")
        );
    }
    else
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("[Mage] Barrier removed voluntarily")
        );
    }
}

void AEnemyMageAI::RemoveBarrier()
{
    RemoveBarrierInternal(false);
}

void AEnemyMageAI::UpdateBarrierState()
{
    if (IsValid(ActiveBarrier) &&
        !ActiveBarrier->IsBarrierActive())
    {
        RemoveBarrierInternal(true);
    }
}

// ==========================================
// COMBAT
// ==========================================

ARICBuilding* AEnemyMageAI::FindCombatTarget() const
{
    if (!GetWorld())
        return nullptr;

    ARICBuilding* CapitolBuilding =
        Cast<ARICBuilding>(Capitol.Get());

    if (IsValid(CapitolBuilding) &&
        CapitolBuilding->IsAlive())
    {
        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            CapitolBuilding->GetActorLocation()
        );

        if (Distance <= AttackRange)
            return CapitolBuilding;
    }

    // Attack walls only when swordsmen attack them.
    for (TActorIterator<AEnemySwordsmanAI>
        It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Swordsman = *It;

        if (!IsValid(Swordsman) ||
            !Swordsman->IsAlive())
        {
            continue;
        }

        ARICBuilding* Wall =
            Swordsman->TargetWall.Get();

        if (!IsValid(Wall) ||
            !Wall->IsAlive() ||
            Wall->BuildingType != ERICBuildingType::Barrier)
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Wall->GetActorLocation()
        );

        if (Distance <= WallAttackDistance)
            return Wall;
    }

    return nullptr;
}

void AEnemyMageAI::StartCombat(
    ARICBuilding* TargetBuilding)
{
    if (!IsValid(TargetBuilding) || !IsAlive())
        return;

    if (bAttacking &&
        CombatTarget.Get() == TargetBuilding)
    {
        return;
    }

    StopCombat();
    CancelBarrierCast();

    if (HasActiveBarrier())
        RemoveBarrierInternal(false);

    CombatTarget = TargetBuilding;
    bAttacking = true;
    MageState = EMageState::Attacking;

    StopMovement();

    GetWorldTimerManager().SetTimer(
        AttackTimer,
        this,
        &AEnemyMageAI::PerformAttack,
        FMath::Max(0.1f, AttackCooldown),
        true
    );

    PerformAttack();
}

void AEnemyMageAI::PerformAttack()
{
    ARICBuilding* Building = CombatTarget.Get();

    if (!IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive())
    {
        StopCombat();
        return;
    }

    const bool bIsCapitol =
        Building == Capitol.Get();

    const float AllowedDistance =
        bIsCapitol ? AttackRange : WallAttackDistance;

    if (FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()) > AllowedDistance)
    {
        StopCombat();
        return;
    }

    if (HasActiveBarrier())
        RemoveBarrierInternal(false);

    FVector Direction =
        Building->GetActorLocation() -
        GetActorLocation();

    Direction.Z = 0.f;

    if (!Direction.IsNearlyZero())
        SetActorRotation(Direction.Rotation());

    if (AttackMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            if (!Anim->Montage_IsPlaying(AttackMontage))
            {
                Anim->Montage_Play(AttackMontage);
            }
        }
    }

    FDamageEvent DamageEvent(
        UDamageType::StaticClass()
    );

    const float AppliedDamage = Building->TakeDamage(
        Damage,
        DamageEvent,
        GetController(),
        this
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Mage] Attack | Target: %s | Damage: %.1f"),
        *GetNameSafe(Building),
        AppliedDamage
    );
}

void AEnemyMageAI::StopCombat()
{
    GetWorldTimerManager().ClearTimer(AttackTimer);

    if (AttackMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(0.15f, AttackMontage);
        }
    }

    bAttacking = false;
    CombatTarget.Reset();

    bFollowingSwordsman = false;
    bMovingToCapitol = false;
}

void AEnemyMageAI::UpdateCombat()
{
    ARICBuilding* TargetBuilding = FindCombatTarget();

    if (IsValid(TargetBuilding))
    {
        StartCombat(TargetBuilding);
    }
    else if (bAttacking)
    {
        StopCombat();
    }
}

// ==========================================
// MOVEMENT
// ==========================================

void AEnemyMageAI::UpdateFollowSpeed(float DeltaTime)
{
    UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    if (!Movement)
        return;

    float TargetSpeed = NormalMovementSpeed;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    if (IsValid(Ally) && Ally->IsAlive())
    {
        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Ally->GetActorLocation()
        );

        const float AllySpeed =
            Ally->GetVelocity().Size2D();

        if (Distance > FollowDistance + 100.f)
        {
            TargetSpeed = MageMovementSpeed;
        }
        else
        {
            TargetSpeed = FMath::Clamp(
                AllySpeed,
                NormalMovementSpeed,
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

void AEnemyMageAI::UpdateMovement()
{
    if (!IsAlive() ||
        !IsValid(Capitol) ||
        bAttacking ||
        bCastingBarrier)
    {
        return;
    }

    AAIController* MageAIController =
        Cast<AAIController>(GetController());

    if (!IsValid(MageAIController))
        return;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    // Follow swordsman.
    if (IsValid(Ally) && Ally->IsAlive())
    {
        MageState = EMageState::FollowingSwordsmen;

        if (!bFollowingSwordsman ||
            CurrentMoveTarget.Get() != Ally)
        {
            CurrentMoveTarget = Ally;
            bFollowingSwordsman = true;
            bMovingToCapitol = false;

            MageAIController->MoveToActor(
                Ally,
                35.0f,
                true,
                true,
                true
            );

            UE_LOG(
                LogTemp,
                Log,
                TEXT("[Mage] Following swordsman: %s"),
                *Ally->GetName()
            );
        }

        return;
    }

    // Move to Capitol.
    MageState = HasActiveBarrier()
        ? EMageState::MovingAlone
        : EMageState::WaitingForBarrier;

    if (!bMovingToCapitol ||
        CurrentMoveTarget.Get() != Capitol.Get())
    {
        CurrentMoveTarget = Capitol.Get();

        bMovingToCapitol = true;
        bFollowingSwordsman = false;

        MageAIController->MoveToActor(
            Capitol.Get(),
            AcceptanceRadius,
            true,
            true,
            true
        );

        UE_LOG(
            LogTemp,
            Log,
            TEXT("[Mage] Moving to Capitol")
        );
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
        GetWorldTimerManager().ClearTimer(BehaviorTimer);
        ReleaseWaitingSwordsmen();
        return;
    }

    FindAlliesAhead();
    SelectSwordsman();

    UpdateBarrierState();

    // Combat has priority over barrier creation.
    UpdateCombat();

    if (!bAttacking && CanCreateBarrier())
    {
        Barrier();
    }

    const float Interval =
        FMath::Max(0.05f, FollowUpdateInterval);

    UpdateFollowSpeed(Interval);
    UpdateMovement();
}

// ==========================================
// DEATH
// ==========================================

void AEnemyMageAI::Die()
{
    if (!IsAlive())
        return;

    GetWorldTimerManager().ClearTimer(BehaviorTimer);

    StopCombat();
    CancelBarrierCast();
    RemoveBarrierInternal(false);

    Super::Die();
}

void AEnemyMageAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(BehaviorTimer);

    StopCombat();
    CancelBarrierCast();
    RemoveBarrierInternal(false);

    Super::EndPlay(EndPlayReason);
}
