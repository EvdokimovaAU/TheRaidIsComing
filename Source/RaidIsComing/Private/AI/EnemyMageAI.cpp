
#include "AI/EnemyMageAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/GameBalanceManager.h"
#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = false;
    bAutoStartMovement = false;
    MovementSpeed = 250.f;
    AcceptanceRadius = 100.f;
}

// ==========================================
// BALANCE
// ==========================================

void AEnemyMageAI::ApplyEnemyBalance()
{
    Super::ApplyEnemyBalance();

    AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this);

    if (!IsValid(Balance))
    {
        return;
    }

    const FMageBalanceSettings& Settings = Balance->Mage;

    BarrierHP = FMath::Max(1.f, Settings.BarrierHealth);
    BarrierCooldown = FMath::Max(0.f, Settings.BarrierCooldown);
    BarrierDuration = FMath::Max(0.f, Settings.BarrierDuration);
    BarrierSpawnDelay = FMath::Max(0.f, Settings.BarrierSpawnDelay);
    AllyDetectionRadius = FMath::Max(0.f, Settings.AllyDetectionRadius);
    FollowDistance = FMath::Max(0.f, Settings.FollowDistance);
    MageAcceleration = FMath::Max(0.f, Settings.MageAcceleration);
    MageBrakingDeceleration =
        FMath::Max(0.f, Settings.MageBrakingDeceleration);
    SpeedChangeRate = FMath::Max(0.f, Settings.SpeedChangeRate);
    FollowUpdateInterval =
        FMath::Max(0.05f, Settings.FollowUpdateInterval);
    NormalMovementSpeed = FMath::Max(0.f, Settings.MovementSpeed);
    MageMovementSpeed =
        FMath::Max(MageMovementSpeed, NormalMovementSpeed);

    MaxProtectedSwordsmen = FMath::Clamp(MaxProtectedSwordsmen, 1, 2);
    RegroupDistance =
        FMath::Max(RegroupDistance, FollowDistance + 50.f);
    AllyReleaseDistance =
        FMath::Max(AllyReleaseDistance, RegroupDistance + 100.f);

    CurrentDesiredSpeed = NormalMovementSpeed;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemyMageAI::BeginPlay()
{
    Super::BeginPlay();

    MaxProtectedSwordsmen = FMath::Clamp(MaxProtectedSwordsmen, 1, 2);

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = NormalMovementSpeed;
        Movement->MaxAcceleration = MageAcceleration;
        Movement->BrakingDecelerationWalking =
            MageBrakingDeceleration;
        Movement->GroundFriction = 4.f;
        Movement->bRequestedMoveUseAcceleration = true;
    }

    CurrentDesiredSpeed = NormalMovementSpeed;

    NextBarrierTime = GetWorld()->GetTimeSeconds() + 0.35f;

    GetWorldTimerManager().SetTimer(
        BehaviorTimer,
        this,
        &AEnemyMageAI::UpdateBehavior,
        FMath::Max(0.05f, FollowUpdateInterval),
        true
    );

    UpdateMovement();
}

// ==========================================
// ALLY SEARCH
// ==========================================

bool AEnemyMageAI::IsSwordsman(const AEnemyAI* Enemy) const
{
    return IsValid(Enemy) &&
        Enemy->IsA(AEnemySwordsmanAI::StaticClass());
}

void AEnemyMageAI::FindAlliesAhead()
{
    AlliesAhead.Empty();

    if (!GetWorld())
    {
        return;
    }

    const float RadiusSq = FMath::Square(AllyDetectionRadius);

    for (TActorIterator<AEnemySwordsmanAI> It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Ally = *It;

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        if (FVector::DistSquared2D(
            GetActorLocation(),
            Ally->GetActorLocation()) <= RadiusSq)
        {
            AlliesAhead.Add(Ally);
        }
    }
}

// ==========================================
// ESCORT GROUP
// ==========================================

void AEnemyMageAI::RefreshEscortGroup()
{
    if (!GetWorld())
    {
        return;
    }

    for (int32 Index = EscortGroup.Num() - 1; Index >= 0; --Index)
    {
        AEnemySwordsmanAI* Ally = EscortGroup[Index].Get();

        if (!IsValid(Ally) ||
            !Ally->IsAlive() ||
            !Ally->IsAssignedToMage(this))
        {
            EscortGroup.RemoveAt(Index);
            continue;
        }

        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Ally->GetActorLocation());

        if (Distance > AllyReleaseDistance &&
            FollowedSwordsman.Get() != Ally)
        {
            Ally->ClearEscortSpeedLimit(this);
            Ally->ReleaseEscortMage(this);
            EscortGroup.RemoveAt(Index);
        }
    }

    if (EscortGroup.Num() >= MaxProtectedSwordsmen ||
        bCastingBarrier || bAttacking)
    {
        return;
    }

    TArray<AEnemySwordsmanAI*> Candidates;

    for (AEnemyAI* Enemy : AlliesAhead)
    {
        AEnemySwordsmanAI* Ally = Cast<AEnemySwordsmanAI>(Enemy);

        if (!IsValid(Ally) ||
            !Ally->IsAlive() ||
            Ally->HasEscortMage() ||
            Ally->IsAttackingWall() ||
            Ally->IsAttackingCapitol())
        {
            continue;
        }

        Candidates.Add(Ally);
    }

    Candidates.Sort(
        [this](const AEnemySwordsmanAI& A,
            const AEnemySwordsmanAI& B)
        {
            return FVector::DistSquared2D(
                GetActorLocation(), A.GetActorLocation()) <
                FVector::DistSquared2D(
                    GetActorLocation(), B.GetActorLocation());
        }
    );

    for (AEnemySwordsmanAI* Ally : Candidates)
    {
        if (EscortGroup.Num() >= MaxProtectedSwordsmen)
        {
            break;
        }

        if (Ally->TryAssignEscortMage(this))
        {
            EscortGroup.Add(Ally);

            UE_LOG(LogTemp, Log,
                TEXT("[Mage] Assigned ally: %s"),
                *Ally->GetName());
        }
    }
}

void AEnemyMageAI::SelectSwordsman()
{
    AEnemySwordsmanAI* Current = FollowedSwordsman.Get();

    if (IsValid(Current) &&
        Current->IsAlive() &&
        Current->IsAssignedToMage(this))
    {
        return;
    }

    RestoreSwordsmanSpeed();
    FollowedSwordsman.Reset();

    AEnemySwordsmanAI* Nearest = nullptr;
    float NearestDistanceSq = TNumericLimits<float>::Max();

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) ||
            !Ally->IsAlive() ||
            !Ally->IsAssignedToMage(this))
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared2D(
            GetActorLocation(),
            Ally->GetActorLocation());

        if (DistanceSq < NearestDistanceSq)
        {
            NearestDistanceSq = DistanceSq;
            Nearest = Ally;
        }
    }

    FollowedSwordsman = Nearest;
    ResetMovementRequest();

    if (IsValid(Nearest))
    {
        UE_LOG(LogTemp, Log,
            TEXT("[Mage] Main ally selected: %s"),
            *Nearest->GetName());
    }
}

AEnemySwordsmanAI* AEnemyMageAI::FindFollowTarget() const
{
    AEnemySwordsmanAI* Best = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) ||
            !Ally->IsAlive() ||
            !Ally->IsAssignedToMage(this))
        {
            continue;
        }

        const float DistanceSq = FVector::DistSquared2D(
            GetActorLocation(),
            Ally->GetActorLocation());

        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            Best = Ally;
        }
    }

    return Best;
}

AEnemySwordsmanAI* AEnemyMageAI::FindRegroupTarget() const
{
    AEnemySwordsmanAI* Farthest = nullptr;
    float FarthestDistance = RegroupDistance;

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        if (Ally->IsAttackingWall() ||
            Ally->IsAttackingCapitol())
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Ally->GetActorLocation());

        if (Distance > FarthestDistance)
        {
            FarthestDistance = Distance;
            Farthest = Ally;
        }
    }

    return Farthest;
}

void AEnemyMageAI::ReleaseEscortGroup()
{
    ReleaseWaitingSwordsmen();
    RestoreAllEscortSpeeds();

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        if (AEnemySwordsmanAI* Ally = Weak.Get())
        {
            Ally->ReleaseEscortMage(this);
        }
    }

    EscortGroup.Empty();
    FollowedSwordsman.Reset();
    ResetMovementRequest();
}

// ==========================================
// WAITING DURING CAST
// ==========================================

void AEnemyMageAI::StopNearbySwordsmen()
{
    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) ||
            !Ally->IsAlive() ||
            !Ally->IsAssignedToMage(this))
        {
            continue;
        }

        if (Ally->IsAttackingWall() ||
            Ally->IsAttackingCapitol())
        {
            continue;
        }

        Ally->WaitForMage(this);

        if (Ally->IsWaitingForMage() &&
            !WaitingSwordsmen.Contains(Ally))
        {
            WaitingSwordsmen.Add(Ally);
        }
    }
}

void AEnemyMageAI::ReleaseWaitingSwordsmen()
{
    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : WaitingSwordsmen)
    {
        if (AEnemySwordsmanAI* Ally = Weak.Get())
        {
            Ally->ResumeAfterMage(this);
        }
    }

    WaitingSwordsmen.Empty();
}

// ==========================================
// GROUP COMBAT
// ==========================================

bool AEnemyMageAI::AreProtectedAlliesAttacking() const
{
    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        const AEnemySwordsmanAI* Ally = Weak.Get();

        if (IsValid(Ally) &&
            Ally->IsAlive() &&
            (Ally->IsAttackingWall() ||
                Ally->IsAttackingCapitol()))
        {
            return true;
        }
    }

    return false;
}

void AEnemyMageAI::UpdateGroupCombatState()
{
    bAlliesNeedToAttack = AreProtectedAlliesAttacking();

    if (!bAlliesNeedToAttack)
    {
        return;
    }

    if (bCastingBarrier)
    {
        CancelBarrierCast();
    }

    if (HasActiveBarrier())
    {
        RemoveBarrierInternal(false);
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
        !bCastingBarrier &&
        !bAttacking &&
        !bAlliesNeedToAttack &&
        !AreProtectedAlliesAttacking() &&
        !HasActiveBarrier() &&
        BarrierClass != nullptr &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= NextBarrierTime;
}

void AEnemyMageAI::Barrier()
{
    if (!CanCreateBarrier())
    {
        return;
    }

    bCastingBarrier = true;
    bBarrierSpawnedThisCast = false;
    MageState = EMageState::CreatingBarrier;

    StopMovement();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    ResetMovementRequest();
    StopNearbySwordsmen();

    float Duration = 0.f;

    if (BarrierMontage && GetMesh())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            Duration = Anim->Montage_Play(BarrierMontage);
        }
    }

    if (Duration <= 0.f)
    {
        SpawnBarrierFromAnimation();
        FinishBarrierCast();
        return;
    }

    const float SpawnDelay = FMath::Clamp(
        BarrierSpawnDelay,
        0.01f,
        Duration
    );

    GetWorldTimerManager().SetTimer(
        BarrierSpawnTimer,
        this,
        &AEnemyMageAI::SpawnBarrierFromAnimation,
        SpawnDelay,
        false
    );

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

    if (AreProtectedAlliesAttacking() ||
        bAlliesNeedToAttack)
    {
        CancelBarrierCast();
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
        HasActiveBarrier() ||
        bAttacking)
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
            FMath::Max(0.5f, BarrierCooldown);

        UE_LOG(LogTemp, Warning,
            TEXT("[Mage] Barrier spawn failed"));
        return;
    }

    ActiveBarrier->ActivateBarrier(
        BarrierHP,
        BarrierDuration
    );

    ActiveBarrier->AttachToMage(this);
    bBarrierDestroyed = false;
}

void AEnemyMageAI::FinishBarrierCast()
{
    if (!bCastingBarrier)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(BarrierSpawnTimer);
    GetWorldTimerManager().ClearTimer(BarrierCastEndTimer);

    if (!bBarrierSpawnedThisCast)
    {
        bBarrierSpawnedThisCast = true;
        SpawnBarrierInternal();
    }

    bCastingBarrier = false;

    ReleaseWaitingSwordsmen();
    ResetMovementRequest();

    MageState = EscortGroup.IsEmpty()
        ? EMageState::MovingAlone
        : EMageState::FollowingSwordsmen;

    UpdateMovement();

    UE_LOG(LogTemp, Log,
        TEXT("[Mage] Barrier cast finished"));
}

void AEnemyMageAI::CancelBarrierCast()
{
    if (!bCastingBarrier)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(BarrierSpawnTimer);
    GetWorldTimerManager().ClearTimer(BarrierCastEndTimer);

    bCastingBarrier = false;
    bBarrierSpawnedThisCast = false;

    if (BarrierMontage && GetMesh())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(0.1f, BarrierMontage);
        }
    }

    ReleaseWaitingSwordsmen();
    ResetMovementRequest();
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

    if (GetWorld())
    {
        NextBarrierTime =
            GetWorld()->GetTimeSeconds() +
            FMath::Max(0.5f, BarrierCooldown);
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
    {
        return nullptr;
    }

    ARICBuilding* CapitolBuilding =
        Cast<ARICBuilding>(Capitol.Get());

    if (IsValid(CapitolBuilding) &&
        CapitolBuilding->IsAlive())
    {
        if (FVector::Dist2D(
            GetActorLocation(),
            CapitolBuilding->GetActorLocation()) <= AttackRange)
        {
            return CapitolBuilding;
        }
    }

    ARICBuilding* ClosestWall = nullptr;
    float ClosestDistance = TNumericLimits<float>::Max();

    for (TActorIterator<ARICBuilding> It(GetWorld()); It; ++It)
    {
        ARICBuilding* Building = *It;

        if (!IsValid(Building) ||
            !Building->IsAlive() ||
            Building->BuildingType != ERICBuildingType::Barrier)
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            GetActorLocation(),
            Building->GetActorLocation());

        if (Distance <= WallAttackDistance &&
            Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestWall = Building;
        }
    }

    return ClosestWall;
}

void AEnemyMageAI::StartCombat(ARICBuilding* TargetBuilding)
{
    if (!IsValid(TargetBuilding) || !IsAlive())
    {
        return;
    }

    if (bAttacking && CombatTarget.Get() == TargetBuilding)
    {
        return;
    }

    StopCombat();
    CancelBarrierCast();

    if (HasActiveBarrier())
    {
        RemoveBarrierInternal(false);
    }

    RestoreAllEscortSpeeds();

    CombatTarget = TargetBuilding;
    bAttacking = true;
    MageState = EMageState::Attacking;

    StopMovement();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    ResetMovementRequest();

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

    const bool bIsCapitol = Building == Capitol.Get();

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
    {
        RemoveBarrierInternal(false);
    }

    FVector Direction =
        Building->GetActorLocation() - GetActorLocation();

    Direction.Z = 0.f;

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    if (AttackMontage && GetMesh())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            if (!Anim->Montage_IsPlaying(AttackMontage))
            {
                Anim->Montage_Play(AttackMontage);
            }
        }
    }

    FDamageEvent DamageEvent(UDamageType::StaticClass());

    Building->TakeDamage(
        Damage,
        DamageEvent,
        GetController(),
        this
    );
}

void AEnemyMageAI::StopCombat()
{
    GetWorldTimerManager().ClearTimer(AttackTimer);

    if (AttackMontage && GetMesh())
    {
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(0.15f, AttackMontage);
        }
    }

    bAttacking = false;
    CombatTarget.Reset();
    ResetMovementRequest();
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
// MOVEMENT REQUEST
// ==========================================

void AEnemyMageAI::ResetMovementRequest()
{
    CurrentMoveTarget.Reset();
    bFollowingSwordsman = false;
    bMovingToCapitol = false;
}

void AEnemyMageAI::RequestMoveToActor(
    AActor* MoveTarget,
    float InAcceptanceRadius)
{
    if (!IsValid(MoveTarget) || !IsAlive())
    {
        return;
    }

    AAIController* MageController =
        Cast<AAIController>(GetController());

    if (!IsValid(MageController))
    {
        return;
    }

    const bool bSameTarget =
        CurrentMoveTarget.Get() == MoveTarget;

    const EPathFollowingStatus::Type Status =
        MageController->GetMoveStatus();

    if (bSameTarget &&
        Status == EPathFollowingStatus::Moving)
    {
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        MoveTarget->GetActorLocation()
    );

    if (bSameTarget && Distance <= InAcceptanceRadius)
    {
        return;
    }

    CurrentMoveTarget = MoveTarget;

    MageController->MoveToActor(
        MoveTarget,
        FMath::Max(10.f, InAcceptanceRadius),
        true,
        true,
        true
    );
}

// ==========================================
// SPEED CONTROL
// ==========================================

void AEnemyMageAI::RestoreAllEscortSpeeds()
{
    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        if (AEnemySwordsmanAI* Ally = Weak.Get())
        {
            Ally->ClearEscortSpeedLimit(this);
        }
    }

    OriginalEscortSpeeds.Empty();
    SpeedControlledSwordsman.Reset();
    SavedSwordsmanSpeed = 0.f;
}

void AEnemyMageAI::RestoreSwordsmanSpeed()
{
    RestoreAllEscortSpeeds();
}

void AEnemyMageAI::UpdateFollowSpeed(float DeltaTime)
{
    UCharacterMovementComponent* MageMove =
        GetCharacterMovement();

    if (!MageMove || DeltaTime <= 0.f)
    {
        return;
    }

    if (EscortGroup.IsEmpty() || bAttacking || bCastingBarrier)
    {
        RestoreAllEscortSpeeds();

        MageMove->MaxWalkSpeed = FMath::FInterpConstantTo(
            MageMove->MaxWalkSpeed,
            NormalMovementSpeed,
            DeltaTime,
            FMath::Max(1.f, SpeedChangeRate)
        );

        return;
    }

    float GroupSpeed = NormalMovementSpeed;

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        GroupSpeed = FMath::Min(
            GroupSpeed,
            Ally->MovementSpeed
        );
    }

    GroupSpeed = FMath::Max(80.f, GroupSpeed);

    const float Radius = HasActiveBarrier()
        ? ActiveBarrier->BarrierRadius
        : 180.f;

    const float SafeRadius = FMath::Max(
        80.f,
        Radius * FMath::Clamp(
            BarrierSafetyFraction,
            0.35f,
            0.8f
        )
    );

    const FVector MageLocation = GetActorLocation();

    const FVector ToGoal = IsValid(Capitol)
        ? (Capitol->GetActorLocation() -
            MageLocation).GetSafeNormal2D()
        : GetActorForwardVector();

    float MageTargetSpeed = GroupSpeed;

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        if (Ally->IsWaitingForMage())
        {
            continue;
        }

        const FVector Offset =
            Ally->GetActorLocation() - MageLocation;

        const float Distance = Offset.Size2D();

        const float Along = FVector::DotProduct(
            Offset,
            ToGoal
        );

        const float Alpha = FMath::Clamp(
            (Distance - SafeRadius * 0.55f) /
            FMath::Max(1.f, SafeRadius * 0.35f),
            0.f,
            1.f
        );

        float AllyTargetSpeed = GroupSpeed;

        if (Along > 0.f)
        {
            AllyTargetSpeed = FMath::Lerp(
                GroupSpeed,
                0.f,
                Alpha
            );

            MageTargetSpeed = FMath::Max(
                MageTargetSpeed,
                FMath::Lerp(
                    GroupSpeed,
                    MageMovementSpeed,
                    Alpha
                )
            );
        }
        else if (Along < 0.f)
        {
            MageTargetSpeed = FMath::Min(
                MageTargetSpeed,
                FMath::Lerp(
                    GroupSpeed,
                    0.f,
                    Alpha
                )
            );
        }

        Ally->SetEscortSpeedLimit(
            this,
            AllyTargetSpeed,
            DeltaTime
        );
    }

    MageMove->MaxWalkSpeed = FMath::FInterpConstantTo(
        MageMove->MaxWalkSpeed,
        MageTargetSpeed,
        DeltaTime,
        FMath::Max(1.f, SpeedChangeRate)
    );
}

// ==========================================
// MOVEMENT
// ==========================================

void AEnemyMageAI::UpdateMovement()
{
    if (!IsAlive() ||
        !IsValid(Capitol) ||
        bCastingBarrier ||
        bAttacking)
    {
        return;
    }

    AEnemySwordsmanAI* Ally = FollowedSwordsman.Get();

    const bool bEscort =
        IsValid(Ally) &&
        Ally->IsAlive() &&
        Ally->IsAssignedToMage(this);

    MageState = bEscort
        ? EMageState::FollowingSwordsmen
        : EMageState::MovingAlone;

    bFollowingSwordsman = bEscort;
    bMovingToCapitol = !bEscort;

    RequestMoveToActor(
        Capitol.Get(),
        AcceptanceRadius
    );
}

void AEnemyMageAI::MoveToTarget()
{
    UpdateMovement();
}

// ==========================================
// MAIN BEHAVIOR
// ==========================================

void AEnemyMageAI::UpdateBehavior()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(BehaviorTimer);
        ReleaseWaitingSwordsmen();
        return;
    }

    UpdateBarrierState();

    FindAlliesAhead();
    RefreshEscortGroup();
    SelectSwordsman();

    if (bCastingBarrier)
    {
        return;
    }

    UpdateGroupCombatState();
    UpdateCombat();

    if (!bAttacking &&
        !bCastingBarrier &&
        CanCreateBarrier())
    {
        Barrier();
    }

    if (bCastingBarrier)
    {
        return;
    }

    const float Interval =
        FMath::Max(0.05f, FollowUpdateInterval);

    UpdateFollowSpeed(Interval);
    UpdateMovement();
}

// ==========================================
// DEATH AND CLEANUP
// ==========================================

void AEnemyMageAI::Die()
{
    if (!IsAlive())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(BehaviorTimer);

    StopCombat();
    CancelBarrierCast();
    ReleaseEscortGroup();
    RemoveBarrierInternal(false);

    Super::Die();
}

void AEnemyMageAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(BehaviorTimer);

    StopCombat();
    CancelBarrierCast();
    ReleaseEscortGroup();
    RemoveBarrierInternal(false);

    Super::EndPlay(EndPlayReason);
}
