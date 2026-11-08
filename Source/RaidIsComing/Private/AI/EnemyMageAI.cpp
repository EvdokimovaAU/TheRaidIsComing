
#include "AI/EnemyMageAI.h"
#include "EnemyCombatNavigation.h"
#include "Components/CapsuleComponent.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/GameBalanceManager.h"
#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
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

// Shared combat geometry: horizontal distance to the visible building bounds.
// A small inset compensates for oversized decorative collision/bounds.
static float GetMageBuildingSurfaceDistance(const AActor* From, const AActor* Building)
{
    return IsValid(From) ? EnemyCombatNavigation::SurfaceDistance(From->GetActorLocation(),
        Cast<ARICBuilding>(Building)) : TNumericLimits<float>::Max();
}


void AEnemyMageAI::OnCombatNavigationChanged(ANavigationData* NavData)
{
    if (NavData) ClearMarchPlan();
    RejectedWalls.Empty();
    FailedApproachWall.Reset();
    FailedApproachAnchors.Empty();
    RejectedFromPosition = GetActorLocation();
    NextSoloDecisionTime = 0.f;
    SoloAvoidedWall.Reset();
}

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bAutoStartMovement = false;
    MovementSpeed = 250.f;
    AcceptanceRadius = 100.f;
    AttackRange = 135.f;
    WallAttackDistance = 135.f;
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

    // Tuned combat ranges (cm). Kept in code as requested.
    AttackRange = 135.f;
    WallAttackDistance = 135.f;

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
    RejectedFromPosition = GetActorLocation();
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        Nav->OnNavigationGenerationFinishedDelegate.AddDynamic(this, &AEnemyMageAI::OnCombatNavigationChanged);

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
        Movement->bOrientRotationToMovement = true;
        Movement->RotationRate = FRotator(0.f, 420.f, 0.f);
        bUseControllerRotationYaw = false;
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

// Escort wall staging: move only to a complete-path NavMesh position.
void AEnemyMageAI::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Obstacle navigation belongs to BehaviorTimer, never to Tick.
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
    if (!GetWorld() || (IsValid(Capitol) &&
        GetMageBuildingSurfaceDistance(this, Capitol.Get()) <= AttackRange + FollowDistance)) return;
    for (TActorIterator<AEnemySwordsmanAI> It(GetWorld()); It; ++It)
        if (It->IsAlive() && !It->HasEscortMage() &&
            FVector::Dist2D(GetActorLocation(), It->GetActorLocation()) <= AllyDetectionRadius)
            AlliesAhead.Add(*It);
}

// ==========================================
// ESCORT GROUP
// ==========================================

bool AEnemyMageAI::IsAllyOnRoute(const AEnemySwordsmanAI* Ally) const
{
    if (!IsValid(Ally) || EscortRoute.IsEmpty()) return false;
    const float MageProgress = ProjectOntoEscortRoute(GetActorLocation(), 0.f, EscortRouteDistances.Last());
    const float At = ProjectOntoEscortRoute(Ally->GetActorLocation(),
        FMath::Max(0.f, MageProgress - FollowDistance), EscortRouteDistances.Last());
    return At >= MageProgress - FollowDistance && At - MageProgress <= AllyDetectionRadius &&
        FVector::Dist2D(Ally->GetActorLocation(), PointOnEscortRoute(At)) <= FollowDistance;
}

void AEnemyMageAI::RefreshEscortGroup()
{
    for (int32 Index = EscortGroup.Num() - 1; Index >= 0; --Index)
    {
        auto* Ally = EscortGroup[Index].Get();
        if (IsValid(Ally) && Ally->IsAlive() && Ally->IsAssignedToMage(this)) continue;
        JoinedEscort.Remove(EscortGroup[Index]);
        MarchDestinations.Remove(EscortGroup[Index]);
        EscortGroup.RemoveAt(Index);
        EscortRoute.Empty();
    }
    if (bCastingBarrier || bAttacking || ObstaclePhase != EObstaclePhase::None) return;
    BuildEscortRoute();
    if (MeetingRecruit.IsValid() && !IsAllyOnRoute(MeetingRecruit.Get()))
        CancelRecruitMeeting(TEXT("route changed"));
    if (EscortGroup.Num() >= FMath::Min(2, MaxProtectedSwordsmen) || MeetingRecruit.IsValid()) return;
    if (!JoinedEscort.IsEmpty() && !HasActiveBarrier()) return;
    for (AEnemyAI* Enemy : AlliesAhead)
    {
        auto* Ally = Cast<AEnemySwordsmanAI>(Enemy);
        if (!IsValid(Ally) || !Ally->IsAlive() || Ally->HasEscortMage() ||
            Ally->IsAttackingWall() || Ally->IsAttackingCapitol()) continue;
        if (ReserveRecruit(Ally)) break;
    }
}

void AEnemyMageAI::SelectSwordsman()
{
    AEnemySwordsmanAI* Current = FollowedSwordsman.Get();

    if (IsValid(Current) &&
        Current->IsAlive() &&
        Current->IsAssignedToMage(this) && JoinedEscort.Contains(Current))
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
            !Ally->IsAssignedToMage(this) || !JoinedEscort.Contains(Weak))
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
    JoinedEscort.Empty();
    bEscortMarchActive = false;
    CancelRecruitMeeting(TEXT("mage released group"));
    ClearMarchPlan();
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

    // Wall engagements are staged under the dome first.
    AEnemySwordsmanAI* Leader = FollowedSwordsman.Get();
    if (IsValid(Leader) && Leader->IsAttackingWall())
        return;

    if (bCastingBarrier || ObstaclePhase != EObstaclePhase::None) return;

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
        (ObstaclePhase == EObstaclePhase::Approaching ||
            (!bAlliesNeedToAttack && !AreProtectedAlliesAttacking())) &&
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
    for (auto& Slot : ObstacleSlots)
        if (auto* Member = Slot.Member.Get())
        {
            Member->StopMovement();
            Member->GetCharacterMovement()->StopMovementImmediately();
            Slot.bMoveIssued = false;
            Slot.LastProgressTime = GetWorld()->GetTimeSeconds();
            LastGroupProgressTime = Slot.LastProgressTime;
        }
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
    bRecruitBarrierRefreshCasting = false;
    if (HasActiveBarrier()) bSpawnGroupNeedsBarrier = false;
    if (ObstaclePhase == EObstaclePhase::Restoring)
    {
        // A failed spawn is retried with its configured cooldown, while everyone waits.
        if (!HasActiveBarrier() && BarrierClass) return;
        ObstaclePhase = EObstaclePhase::None;
        bWaitingForPostCombatBarrier = false;
        bCombatWasWall = false;
        UE_LOG(LogTemp, Log, TEXT("[Mage] Barrier restored; rebuilding group route"));
    }

    UpdateFollowSpeed(FMath::Max(0.05f, FollowUpdateInterval));
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
        NextBarrierTime = bDestroyed
            ? GetWorld()->GetTimeSeconds() + FMath::Max(0.f, BarrierCooldown)
            : GetWorld()->GetTimeSeconds();
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
        if (GetMageBuildingSurfaceDistance(this, CapitolBuilding) <= AttackRange)
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

        const float Distance = GetMageBuildingSurfaceDistance(this, Building);

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
    UE_LOG(LogTemp, Log, TEXT("[Mage] Combat started: %s"), *GetNameSafe(TargetBuilding));
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

    if (!EnemyCombatNavigation::CanAttack(this, Building, AllowedDistance))
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

bool AEnemyMageAI::OwnsEscortMovement() const
{
    return bEscortMarchActive || bCastingBarrier || bWaitingForPostCombatBarrier ||
        bRecruitBarrierRefreshCasting || ObstaclePhase != EObstaclePhase::None;
}

bool AEnemyMageAI::CanSelectEscortObstacle(const AEnemySwordsmanAI* Ally) const
{
    return !bCastingBarrier && !bWaitingForPostCombatBarrier &&
        ObstaclePhase == EObstaclePhase::None &&
        (!FollowedSwordsman.IsValid() || FollowedSwordsman.Get() == Ally);
}

bool AEnemyMageAI::StageEscortAtWall(ARICBuilding* Wall)
{
    if (ObstaclePhase == EObstaclePhase::None) BeginObstacleAction(Wall);
    return ObstaclePhase == EObstaclePhase::Attacking;
}

bool AEnemyMageAI::BeginObstacleAction(ARICBuilding* Wall)
{
    if (!IsValid(Wall) || !Wall->IsAlive() || bCastingBarrier) return false;
    const float Now = GetWorld()->GetTimeSeconds();
    if (RejectedWalls.Contains(Wall)) return false;

    TArray<FObstacleSlot> Planned;
    TArray<FVector> Reserved;
    FVector MageDestination;
    const float SafeRadius = HasActiveBarrier()
        ? ActiveBarrier->BarrierRadius * FMath::Clamp(BarrierSafetyFraction, 0.35f, 0.8f)
        : RegroupDistance;
    float LargestRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    for (const auto& Weak : EscortGroup)
        if (auto* Ally = Weak.Get()) LargestRadius = FMath::Max(LargestRadius,
            Ally->GetCapsuleComponent()->GetScaledCapsuleRadius());
    if (FailedApproachWall.Get() != Wall)
    {
        FailedApproachWall = Wall;
        FailedApproachAnchors.Empty();
    }
    TArray<FVector> RejectedAnchors = FailedApproachAnchors;
    bool bPlanFound = false;
    // If the nearest mage slot leaves no space for a swordsman, try another
    // complete-path anchor on the accessible side before rejecting this wall.
    for (int32 Attempt = 0; Attempt < 8 && !bPlanFound; ++Attempt)
    {
        if (!EnemyCombatNavigation::FindApproach(this, Wall, WallAttackDistance,
            MageDestination, RejectedAnchors, LargestRadius)) break;
        RejectedAnchors.Add(MageDestination);
        Planned.Empty(); Reserved.Empty();
        Reserved.Add(MageDestination);
        FObstacleSlot MageSlot;
        MageSlot.Member = this;
        MageSlot.Destination = MageDestination;
        Planned.Add(MageSlot);
        bPlanFound = true;
        for (const auto& Weak : EscortGroup)
        {
            auto* Ally = Weak.Get();
            if (!IsValid(Ally) || !Ally->IsAlive() || !JoinedEscort.Contains(Weak)) continue;
            FVector Destination;
            if (!EnemyCombatNavigation::FindApproach(Ally, Wall, Ally->WallAttackDistance,
                Destination, Reserved, LargestRadius * 2.f + 16.f, &MageDestination, SafeRadius))
            {
                bPlanFound = false;
                break;
            }
            FObstacleSlot Slot;
            Slot.Member = Ally;
            Slot.Destination = Destination;
            Planned.Add(Slot);
            Reserved.Add(Destination);
        }
    }
    if (!bPlanFound)
    {
        RejectedWalls.Add(Wall, Now);
        UE_LOG(LogTemp, Warning, TEXT("[Mage] Rejected %s: no separated full-path positions for group"),
            *GetNameSafe(Wall));
        return false;
    }
    for (int32 Index = EscortGroup.Num() - 1; Index >= 0; --Index)
    {
        if (JoinedEscort.Contains(EscortGroup[Index])) continue;
        if (auto* ReservedAlly = EscortGroup[Index].Get()) ReservedAlly->ReleaseEscortMage(this);
        EscortGroup.RemoveAt(Index);
    }
    bRecruitBarrierRefreshPending = false;
    // Commit atomically only after all members have reachable positions.
    bEscortMarchActive = false;
    CancelRecruitMeeting(TEXT("group obstacle action started"));
    EscortStagingWall = Wall;
    ObstaclePhase = EObstaclePhase::Approaching;
    ObstacleSlots = MoveTemp(Planned);
    LastGroupProgressTime = Now;
    StopCombat();
    RestoreAllEscortSpeeds();
    StopMovement();
    ResetMovementRequest();
    for (auto& Slot : ObstacleSlots)
    {
        auto* Member = Slot.Member.Get();
        Slot.LastPosition = Member->GetActorLocation();
        Slot.LastProgressTime = Now;
        Member->StopMovement();
        if (auto* Ally = Cast<AEnemySwordsmanAI>(Member))
        {
            Ally->StopWallAttack();
            Ally->StopWallAvoidance();
            Ally->StopCapitolAttack();
            Ally->SavedWalkSpeed = FMath::Max(Ally->MovementSpeed,
                Ally->GetCharacterMovement()->MaxWalkSpeed);
            Ally->WaitingForMage.Reset();
            Ally->TargetWall = Wall;
            Ally->bAttackingWall = true;
            Ally->bEscortWallAttackAuthorized = false;
        }
        UE_LOG(LogTemp, Log, TEXT("[Mage] Wall %s: complete path confirmed for %s to %s"),
            *Wall->GetName(), *Member->GetName(), *Slot.Destination.ToString());
    }
    return true;
}

void AEnemyMageAI::ResetObstacleAction(const TCHAR* Reason, bool bRestoreBarrier)
{
    UE_LOG(LogTemp, Log, TEXT("[Mage] Reset wall %s: %s"), *GetNameSafe(EscortStagingWall.Get()), Reason);
    StopCombat();
    StopMovement();
    ClearMarchPlan();
    GetCharacterMovement()->MaxWalkSpeed = NormalMovementSpeed;
    for (const auto& Weak : EscortGroup)
        if (auto* Ally = Weak.Get())
        {
            Ally->StopMovement();
            Ally->StopWallAttack();
            Ally->StopCapitolAttack();
        }
    EscortStagingWall.Reset();
    ObstacleSlots.Empty();
    bAlliesNeedToAttack = false;
    ObstaclePhase = bRestoreBarrier ? EObstaclePhase::Restoring : EObstaclePhase::None;
    bWaitingForPostCombatBarrier = bRestoreBarrier;
    if (bRestoreBarrier)
    {
        if (HasActiveBarrier()) RemoveBarrierInternal(false);
        StopNearbySwordsmen();
    }
    ResetMovementRequest();
}

void AEnemyMageAI::UpdateObstacleAction()
{
    if (ObstaclePhase == EObstaclePhase::Restoring)
    {
        if (!bCastingBarrier && CanCreateBarrier()) Barrier();
        // Missing barrier Blueprint must not freeze the entire escort.
        if (!BarrierClass)
        {
            ObstaclePhase = EObstaclePhase::None;
            bWaitingForPostCombatBarrier = false;
            ReleaseWaitingSwordsmen();
        }
        return;
    }
    auto* Wall = EscortStagingWall.Get();
    if (!IsValid(Wall) || !Wall->IsAlive())
    {
        ResetObstacleAction(TEXT("wall destroyed or removed"), true);
        return;
    }
    if (ObstaclePhase == EObstaclePhase::Approaching && !HasActiveBarrier() && CanCreateBarrier())
    {
        Barrier();
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    ObstacleSlots.RemoveAll([](const FObstacleSlot& Slot)
    {
        return !Slot.Member.IsValid() || !Slot.Member->IsAlive();
    });
    bool bReady = true;
    bool bNeedReplan = false;
    for (auto& Slot : ObstacleSlots)
        if (FVector::Dist2D(Slot.Member->GetActorLocation(), Slot.LastPosition) > 5.f)
        {
            Slot.LastPosition = Slot.Member->GetActorLocation();
            LastGroupProgressTime = Now;
        }
    const float SafeRadius = HasActiveBarrier()
        ? ActiveBarrier->BarrierRadius * FMath::Clamp(BarrierSafetyFraction, 0.35f, 0.8f)
        : RegroupDistance;
    for (auto& Slot : ObstacleSlots)
    {
        auto* Member = Slot.Member.Get();
        auto* Ally = Cast<AEnemySwordsmanAI>(Member);
        const float Range = Ally ? Ally->WallAttackDistance : WallAttackDistance;
        // Readiness uses exactly the same range and collision test as damage.
        const bool bAtSlot = FVector::Dist2D(Member->GetActorLocation(), Slot.Destination) <=
            EnemyCombatNavigation::AcceptanceRadius + 2.f;
        const bool bInRange = EnemyCombatNavigation::CanAttack(Member, Wall, Range);
        const bool bInside = Member == this || FVector::Dist2D(Member->GetActorLocation(), GetActorLocation()) <= SafeRadius;
        const bool bPositionReady = bAtSlot && bInRange;
        const bool bArrived = bPositionReady && bInside;
        if (bPositionReady && !Slot.bArrived)
        {
            UE_LOG(LogTemp, Log, TEXT("[Mage] %s reached wall attack slot"), *Member->GetName());
            Member->StopMovement();
        }
        Slot.bArrived = bPositionReady;
        bReady &= bArrived;
        if (ObstaclePhase == EObstaclePhase::Attacking) continue;
        if (bPositionReady) continue;
        auto* AI = Cast<AAIController>(Member->GetController());
        if (!AI) { bNeedReplan = true; continue; }
        // Judge progress for the whole group: an ally yielding to the mage
        // is not stuck while another member is still advancing.
        if (Now - LastGroupProgressTime > 6.f) { bNeedReplan = true; continue; }
        FVector Direction = (Slot.Destination - Member->GetActorLocation()).GetSafeNormal2D();
        if (Slot.bMoveIssued && AI->GetPathFollowingComponent())
            Direction = AI->GetPathFollowingComponent()->GetCurrentDirection().GetSafeNormal2D();
        float Speed = FMath::Min(NormalMovementSpeed, Ally ? Ally->MovementSpeed : NormalMovementSpeed);
        const float Interval = FMath::Max(0.05f, FollowUpdateInterval);
        for (const auto& Other : ObstacleSlots)
        {
            if (Other.Member == Slot.Member || (Member != this && Other.Member.Get() != this)) continue;
            const FVector Offset = Member->GetActorLocation() - Other.Member->GetActorLocation();
            const float Gap = Offset.Size2D();
            const float Away = FVector::DotProduct(Direction, Offset.GetSafeNormal2D());
            if (Away > 0.f)
                Speed = FMath::Min(Speed, FMath::Max(0.f, SafeRadius - Gap) / (Interval * Away));
        }
        // Limit speed without cancelling path following. The same MoveTo stays
        // alive while a straggler catches up; no step/stop/reissue cycle.
        Member->GetCharacterMovement()->MaxWalkSpeed = Speed;
        if (Slot.bMoveIssued)
        {
            if (AI->GetMoveStatus() != EPathFollowingStatus::Moving) bNeedReplan = true;
            continue;
        }
        if (!EnemyCombatNavigation::HasCompletePath(Member, Slot.Destination)) { bNeedReplan = true; continue; }
        Slot.bMoveIssued = true;
        const auto Result = AI->MoveToLocation(Slot.Destination, EnemyCombatNavigation::AcceptanceRadius,
            false, true, false, false, nullptr, false);
        bNeedReplan |= Result == EPathFollowingRequestResult::Failed;
    }
    if (bNeedReplan)
    {
        if (!ObstacleSlots.IsEmpty()) FailedApproachAnchors.Add(ObstacleSlots[0].Destination);
        ResetObstacleAction(TEXT("approach failed, capsule blocked or navigation changed"), false);
        if (FailedApproachAnchors.Num() < 3 && BeginObstacleAction(Wall)) return;
        RejectedWalls.Add(Wall, Now);
        return;
    }
    if (!bReady && ObstaclePhase == EObstaclePhase::Attacking)
    {
        // Revoke all damage before replanning; no member attacks alone.
        ResetObstacleAction(TEXT("attack position lost"), false);
        BeginObstacleAction(Wall);
        return;
    }
    if (!bReady || ObstaclePhase == EObstaclePhase::Attacking) return;
    UE_LOG(LogTemp, Log, TEXT("[Mage] Group ready at %s; removing barrier"), *Wall->GetName());
    if (HasActiveBarrier()) RemoveBarrierInternal(false);
    ObstaclePhase = EObstaclePhase::Attacking;
    // Authorize every member before the first hit, which may destroy the wall.
    for (const auto& Slot : ObstacleSlots)
        if (auto* Ally = Cast<AEnemySwordsmanAI>(Slot.Member.Get())) Ally->bEscortWallAttackAuthorized = true;
    // The leader lands the first hit.
    if (auto* Leader = FollowedSwordsman.Get()) Leader->AuthorizeEscortWallAttack(this, Wall);
    for (const auto& Slot : ObstacleSlots)
        if (auto* Ally = Cast<AEnemySwordsmanAI>(Slot.Member.Get()))
            if (Ally != FollowedSwordsman.Get() && IsValid(Wall) && Wall->IsAlive())
                Ally->AuthorizeEscortWallAttack(this, Wall);
    if (IsValid(Wall) && Wall->IsAlive())
    {
        bCombatWasWall = true;
        StartCombat(Wall);
    }
    UE_LOG(LogTemp, Log, TEXT("[Mage] Group obstacle attack started"));
}

void AEnemyMageAI::UpdateCombat()
{
    if (OwnsEscortMovement() && !bEscortMarchActive) return;
    bool bGroupAtCapitol = false;
    for (const auto& Weak : EscortGroup)
        bGroupAtCapitol |= Weak.IsValid() && Weak->IsAlive() &&
            JoinedEscort.Contains(Weak) && Weak->IsAttackingCapitol();
    if (bGroupAtCapitol)
    {
        bEscortMarchActive = false;
        RestoreAllEscortSpeeds();
        GetCharacterMovement()->MaxWalkSpeed = NormalMovementSpeed;
        auto* Building = Cast<ARICBuilding>(Capitol.Get());
        if (EnemyCombatNavigation::CanAttack(this, Building, AttackRange)) StartCombat(Building);
        else
        {
            // An ally attacking is a command to join combat, not a reason to
            // skip the mage's remaining approach on subsequent behavior updates.
            RequestMoveToActor(Building, AttackRange);
        }
        return;
    }
    auto* Leader = FollowedSwordsman.Get();
    if (IsValid(Leader) && Leader->IsAlive())
    {
        if (Leader->IsAttackingWall())
        {
            auto* Wall = Leader->TargetWall.Get();
            if (!BeginObstacleAction(Wall))
            {
                Leader->StopMovement();
                Leader->StopWallAttack();
                Leader->RejectedWalls.Add(Wall, GetWorld()->GetTimeSeconds());
            }
            return;
        }
        if (Leader->IsAttackingCapitol() && EnemyCombatNavigation::CanAttack(this,
            Cast<ARICBuilding>(Capitol.Get()), AttackRange))
        {
            StartCombat(Cast<ARICBuilding>(Capitol.Get()));
        }
        return;
    }
    auto* CapitolBuilding = Cast<ARICBuilding>(Capitol.Get());
    if (!IsValid(CapitolBuilding) || !CapitolBuilding->IsAlive()) return;
    if (EnemyCombatNavigation::CanAttack(this, CapitolBuilding, AttackRange))
    {
        StartCombat(CapitolBuilding);
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextSoloDecisionTime) return;
    NextSoloDecisionTime = Now + 1.f;
    FVector CapitolApproach;
    const bool bBlocked = !EnemyCombatNavigation::FindApproach(this, CapitolBuilding, AttackRange, CapitolApproach);
    TArray<ARICBuilding*> Walls;
    for (TActorIterator<ARICBuilding> It(GetWorld()); It; ++It)
    {
        auto* Wall = *It;
        if (!Wall->IsAlive() || Wall->BuildingType != ERICBuildingType::Barrier) continue;
        if (RejectedWalls.Contains(Wall)) continue;
        if (!bBlocked && (SoloAvoidedWall.Get() == Wall ||
            GetMageBuildingSurfaceDistance(this, Wall) > WallDetectionRadius)) continue;
        Walls.Add(Wall);
    }
    Walls.Sort([this](const ARICBuilding& A, const ARICBuilding& B)
    {
        return GetMageBuildingSurfaceDistance(this, &A) < GetMageBuildingSurfaceDistance(this, &B);
    });
    for (auto* Wall : Walls)
    {
        if (!bBlocked && FMath::FRandRange(0.f, 100.f) >= SoloWallAttackChance)
        {
            SoloAvoidedWall = Wall;
            break;
        }
        if (BeginObstacleAction(Wall)) break;
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

void AEnemyMageAI::ClearMarchPlan()
{
    EscortRoute.Empty();
    EscortRouteDistances.Empty();
    MarchMoves.Empty();
    bFinishingEscortMarch = false;
    MarchBuilding.Reset();
    MarchDestinations.Empty();
}

bool AEnemyMageAI::EnsureMarchAnchor()
{
    auto* Building = Cast<ARICBuilding>(Capitol.Get());
    if (!IsValid(Building) || !Building->IsAlive()) return false;
    if (MarchBuilding.Get() == Building) return true;
    FVector Destination;
    if (!EnemyCombatNavigation::FindApproach(this, Building, AttackRange, Destination)) return false;
    MarchBuilding = Building;
    MarchAnchor = Destination;
    MarchDestinations.Empty();
    return true;
}

bool AEnemyMageAI::FindEscortMarchDestination(AEnemySwordsmanAI* Ally, FVector& Destination)
{
    if (!IsValid(Ally) || !Ally->IsAssignedToMage(this) || !JoinedEscort.Contains(Ally) ||
        !EnsureMarchAnchor()) return false;
    if (const FVector* Existing = MarchDestinations.Find(Ally))
    {
        Destination = *Existing;
        return true;
    }
    TArray<FVector> Reserved{MarchAnchor};
    for (const auto& Entry : MarchDestinations)
        if (Entry.Key.IsValid() && Entry.Key->IsAlive()) Reserved.Add(Entry.Value);
    const float Separation = GetCapsuleComponent()->GetScaledCapsuleRadius() +
        Ally->GetCapsuleComponent()->GetScaledCapsuleRadius() + 12.f;
    const float Radius = HasActiveBarrier() ? ActiveBarrier->BarrierRadius : RegroupDistance;
    if (!EnemyCombatNavigation::FindApproach(Ally, MarchBuilding.Get(), Ally->AttackRange,
        Destination, Reserved, Separation, &MarchAnchor,
        Radius * FMath::Clamp(BarrierSafetyFraction, 0.35f, 0.8f))) return false;
    MarchDestinations.Add(Ally, Destination);
    return true;
}

void AEnemyMageAI::RequestMoveToActor(AActor* MoveTarget, float InAcceptanceRadius)
{
    if (!IsValid(MoveTarget) || !IsAlive() || OwnsEscortMovement()) return;
    auto* AI = Cast<AAIController>(GetController());
    if (!AI) return;
    if (CurrentMoveTarget.Get() == MoveTarget && AI->GetMoveStatus() == EPathFollowingStatus::Moving) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextWallNavRetryTime) return;
    NextWallNavRetryTime = Now + 1.f;
    FVector Destination;
    auto* Building = Cast<ARICBuilding>(MoveTarget);
    if (MoveTarget == Capitol.Get())
    {
        if (!EnsureMarchAnchor()) return;
        Destination = MarchAnchor;
    }
    else if (!EnemyCombatNavigation::FindApproach(this, Building, WallAttackDistance, Destination)) return;
    CurrentMoveTarget = MoveTarget;
    const auto Result = AI->MoveToLocation(Destination, EnemyCombatNavigation::AcceptanceRadius,
        false, true, false, false, nullptr, false);
    if (Result == EPathFollowingRequestResult::RequestSuccessful)
        UE_LOG(LogTemp, Log, TEXT("[Mage] Route rebuilt to %s: %s"), *GetNameSafe(MoveTarget), *Destination.ToString());
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
    if (bEscortMarchActive)
    {
        // Cast completion can precede the next corridor update. Preserve the
        // group's speed cap while releasing the waiting members, including
        // pairs bound before BeginPlay initializes character movement.
        float Limit = NormalMovementSpeed;
        for (const auto& Weak : JoinedEscort)
            if (Weak.IsValid() && Weak->IsAlive()) Limit = FMath::Min(Limit, Weak->MovementSpeed);
        if (auto* Movement = GetCharacterMovement())
            Movement->MaxWalkSpeed = FMath::Min(Movement->MaxWalkSpeed, Limit);
        return;
    }
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

        if (!IsValid(Ally) || !Ally->IsAlive() || !JoinedEscort.Contains(Weak))
        {
            continue;
        }

        GroupSpeed = FMath::Min(
            GroupSpeed,
            Ally->MovementSpeed
        );
    }

    GroupSpeed = FMath::Max(0.f, GroupSpeed);

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

    FVector ToGoal = IsValid(Capitol)
        ? (Capitol->GetActorLocation() -
            MageLocation).GetSafeNormal2D()
        : GetActorForwardVector();

    if (auto* AI = Cast<AAIController>(GetController()))
        if (AI->GetMoveStatus() == EPathFollowingStatus::Moving && AI->GetPathFollowingComponent())
            ToGoal = AI->GetPathFollowingComponent()->GetCurrentDirection().GetSafeNormal2D();
    float MageTargetSpeed = GroupSpeed;

    for (const TWeakObjectPtr<AEnemySwordsmanAI>& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        if (!JoinedEscort.Contains(Weak) || Ally->IsWaitingForMage())
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
            (Distance - SafeRadius * 0.75f) /
            FMath::Max(1.f, SafeRadius * 0.25f),
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

            // Never accelerate the mage to catch a swordsman.
            MageTargetSpeed = FMath::Min(MageTargetSpeed, GroupSpeed);
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

    MageTargetSpeed = FMath::Min(MageTargetSpeed, NormalMovementSpeed);
    MageMove->MaxWalkSpeed = FMath::FInterpConstantTo(
        FMath::Min(MageMove->MaxWalkSpeed, GroupSpeed),
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
    if (OwnsEscortMovement()) return;
    if (!IsAlive() ||
        !IsValid(Capitol) ||
        bCastingBarrier ||
        bAttacking ||
        bWaitingForPostCombatBarrier)
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

    // The mage should not push into the Capitol mesh while waiting for
    // the leader to initiate combat.
    if (GetMageBuildingSurfaceDistance(this, Capitol.Get()) <= AttackRange)
    {
        StopMovement();
        ResetMovementRequest();
        return;
    }
    if (bEscort && Ally->IsAttackingWall()) return;
    RequestMoveToActor(Capitol.Get(), AcceptanceRadius);
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

    if (FVector::Dist2D(GetActorLocation(), RejectedFromPosition) > 100.f)
        OnCombatNavigationChanged(nullptr);
    UpdateBarrierState();
    if (bCastingBarrier) return;
    if (bSpawnGroupNeedsBarrier)
    {
        if (CanCreateBarrier()) Barrier();
        return;
    }
    if (ObstaclePhase != EObstaclePhase::None)
    {
        RefreshEscortGroup();
        SelectSwordsman();
        UpdateObstacleAction();
        return;
    }
    FindAlliesAhead();
    RefreshEscortGroup();
    SelectSwordsman();

    if (bRecruitBarrierRefreshCasting)
    {
        StopMovement();
        if (!bCastingBarrier && CanCreateBarrier())
        {
            Barrier();
        }
        return;
    }

    // An escort cannot deadlock the Capitol attack by keeping allies waiting.
    if (IsValid(Capitol))
    {
        const float Distance = GetMageBuildingSurfaceDistance(this, Capitol.Get());
        if (Distance <= FMath::Max(AttackRange, FollowDistance))
        {
            if (!bRecruitBarrierRefreshPending) ReleaseWaitingSwordsmen();
        }
    }

    // Combat decisions take priority over barrier maintenance.
    UpdateGroupCombatState();
    UpdateCombat();
    if (ObstaclePhase != EObstaclePhase::None) return;
    if (bAttacking || AreProtectedAlliesAttacking()) return;
    if (MeetingRecruit.IsValid() && UpdateRecruitMeeting()) return;
    if (bCastingBarrier) return;

    bool bAssembled = true;
    for (const auto& Weak : EscortGroup)
    {
        AEnemySwordsmanAI* Ally = Weak.Get();
        if (!IsValid(Ally) || !Ally->IsAlive()) continue;
        if (!JoinedEscort.Contains(Weak)) continue;
        if (Ally->IsWaitingForMage() &&
            FVector::Dist2D(GetActorLocation(), Ally->GetActorLocation()) > FollowDistance)
        {
            bAssembled = false;
        }
    }

    if (bAssembled && !HasActiveBarrier() && CanCreateBarrier())
    {
        Barrier();
        return;
    }
    // Reserved allies stay still until the replacement dome has been cast.
    if (bAssembled && !bWaitingForPostCombatBarrier && !bRecruitBarrierRefreshPending &&
        (HasActiveBarrier() || !BarrierClass || bBarrierDestroyed))
    {
        ReleaseWaitingSwordsmen();
    }

    if (bWaitingForPostCombatBarrier)
    {
        if (CanCreateBarrier())
        {
            bWaitingForPostCombatBarrier = false;
            Barrier();
        }
        return;
    }

    if (bAssembled && CanCreateBarrier())
    {
        Barrier();
    }
    if (bCastingBarrier) return;

    if (!EscortStagingWall.IsValid())
    {
        if (!JoinedEscort.IsEmpty()) UpdateEscortMarch();
        else UpdateMovement();
    }
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
