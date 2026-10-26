
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemySwordsmanAnimInstance.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemySwordsmanAI::AEnemySwordsmanAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemySwordsmanAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        CapitolCheckTimer,
        this,
        &AEnemySwordsmanAI::CheckCapitolDistance,
        0.2f,
        true
    );

    GetWorldTimerManager().SetTimer(
        WallCheckTimer,
        this,
        &AEnemySwordsmanAI::CheckWallAhead,
        FMath::Max(WallCheckInterval, 0.05f),
        true
    );
}

// ==========================================
// CHECK IF ACTOR IS A VALID WALL
// ==========================================

bool AEnemySwordsmanAI::IsValidWall(AActor* Actor) const
{
    const ARICBuilding* Building =
        Cast<ARICBuilding>(Actor);

    if (!IsValid(Building))
    {
        return false;
    }

    return
        Building->BuildingType == ERICBuildingType::Barrier &&
        Building->IsAlive();
}

// ==========================================
// CHECK WALL AHEAD
// ==========================================

void AEnemySwordsmanAI::CheckWallAhead()
{
    if (!IsAlive() ||
        bAttackingWall ||
        bAttackingCapitol ||
        !IsValid(Capitol))
    {
        return;
    }

    const FVector Start =
        GetActorLocation() + FVector(0.0f, 0.0f, 45.0f);

    FVector Direction =
        Capitol->GetActorLocation() - GetActorLocation();

    Direction.Z = 0.0f;

    if (!Direction.Normalize())
    {
        return;
    }

    const FVector End =
        Start + Direction * WallDetectionDistance;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.bTraceComplex = false;

    TArray<FHitResult> HitResults;

    const bool bHit =
        GetWorld()->SweepMultiByChannel(
            HitResults,
            Start,
            End,
            FQuat::Identity,
            ECC_Visibility,
            FCollisionShape::MakeSphere(35.0f),
            QueryParams
        );

    if (!bHit)
    {
        return;
    }

    for (const FHitResult& Hit : HitResults)
    {
        AActor* HitActor = Hit.GetActor();

        if (IsValidWall(HitActor))
        {
            OnObstacleDetected(HitActor);
            return;
        }
    }
}

// ==========================================
// OBSTACLE DETECTED
// ==========================================

void AEnemySwordsmanAI::OnObstacleDetected(AActor* Obstacle)
{
    if (!IsAlive() || !IsValidWall(Obstacle))
    {
        return;
    }

    StopCapitolAttack();
    StopMovement();

    StartWallAttack(Obstacle);
}

// ==========================================
// START WALL ATTACK
// ==========================================

void AEnemySwordsmanAI::StartWallAttack(AActor* Wall)
{
    if (!IsAlive() || !IsValidWall(Wall))
    {
        return;
    }

    if (bAttackingWall && TargetWall.Get() == Wall)
    {
        return;
    }

    StopWallAttack();

    TargetWall = Cast<ARICBuilding>(Wall);
    bAttackingWall = true;

    StopMovement();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall detected: %s"),
        *Wall->GetName()
    );

    GetWorldTimerManager().SetTimer(
        WallAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackWall,
        FMath::Max(WallAttackCooldown, 0.01f),
        true
    );
}

// ==========================================
// ATTACK WALL
// ==========================================

void AEnemySwordsmanAI::AttackWall()
{
    if (!IsAlive())
    {
        StopWallAttack();
        return;
    }

    ARICBuilding* Wall = TargetWall.Get();

    if (!IsValidWall(Wall))
    {
        OnWallDestroyed();
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Wall->GetActorLocation()
    );

    const float AllowedDistance =
        FMath::Max(
            AttackRange,
            WallDetectionDistance
        );

    if (Distance > AllowedDistance)
    {
        StopWallAttack();
        MoveToTarget();
        return;
    }

    PlayAttackAnimation();

    FDamageEvent DamageEvent(UDamageType::StaticClass());

    const float AppliedDamage =
        Wall->TakeDamage(
            WallDamage,
            DamageEvent,
            GetController(),
            this
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall hit: %s | Damage: %.1f"),
        *Wall->GetName(),
        AppliedDamage
    );

    if (!IsValid(Wall) || !Wall->IsAlive())
    {
        OnWallDestroyed();
    }
}

// ==========================================
// STOP WALL ATTACK
// ==========================================

void AEnemySwordsmanAI::StopWallAttack()
{
    GetWorldTimerManager().ClearTimer(WallAttackTimer);

    bAttackingWall = false;
    TargetWall = nullptr;

    StopAttackAnimation();
}

// ==========================================
// WALL DESTROYED
// ==========================================

void AEnemySwordsmanAI::OnWallDestroyed()
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall destroyed. Resuming movement.")
    );

    StopWallAttack();

    if (IsAlive())
    {
        MoveToTarget();
    }
}

// ==========================================
// CHECK CAPITOL DISTANCE
// ==========================================

void AEnemySwordsmanAI::CheckCapitolDistance()
{
    if (!IsAlive() ||
        bAttackingCapitol ||
        bAttackingWall)
    {
        return;
    }

    ARICBuilding* Building =
        Cast<ARICBuilding>(Capitol.Get());

    if (!IsValid(Building) || !Building->IsAlive())
    {
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()
    );

    if (Distance <= AttackRange)
    {
        StartCapitolAttack();
    }
}

// ==========================================
// START CAPITOL ATTACK
// ==========================================

void AEnemySwordsmanAI::StartCapitolAttack()
{
    ARICBuilding* Building =
        Cast<ARICBuilding>(Capitol.Get());

    if (bAttackingCapitol ||
        bAttackingWall ||
        !IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive())
    {
        return;
    }

    bAttackingCapitol = true;

    StopMovement();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Reached Capitol. Attack started!")
    );

    GetWorldTimerManager().SetTimer(
        CapitolAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackCapitol,
        FMath::Max(AttackCooldown, 0.01f),
        true
    );
}

// ==========================================
// ATTACK CAPITOL
// ==========================================

void AEnemySwordsmanAI::AttackCapitol()
{
    ARICBuilding* Building =
        Cast<ARICBuilding>(Capitol.Get());

    if (!IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive())
    {
        StopCapitolAttack();
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()
    );

    if (Distance > AttackRange)
    {
        StopCapitolAttack();
        MoveToTarget();
        return;
    }

    PlayAttackAnimation();

    FDamageEvent DamageEvent(UDamageType::StaticClass());

    const float AppliedDamage =
        Building->TakeDamage(
            Damage,
            DamageEvent,
            GetController(),
            this
        );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Capitol hit! Damage: %.1f"),
        AppliedDamage
    );

    if (!IsValid(Building) || !Building->IsAlive())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Capitol destroyed!")
        );

        StopCapitolAttack();
    }
}

// ==========================================
// STOP CAPITOL ATTACK
// ==========================================

void AEnemySwordsmanAI::StopCapitolAttack()
{
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    bAttackingCapitol = false;

    StopAttackAnimation();
}

// ==========================================
// PLAY ATTACK ANIMATION
// ==========================================

void AEnemySwordsmanAI::PlayAttackAnimation()
{
    if (!IsAlive())
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (!MeshComponent)
    {
        return;
    }

    UEnemySwordsmanAnimInstance* AnimInstance =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (AnimInstance)
    {
        AnimInstance->PlayAttackAnimation();
    }
}

// ==========================================
// STOP ATTACK ANIMATION
// ==========================================

void AEnemySwordsmanAI::StopAttackAnimation()
{
    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (!MeshComponent)
    {
        return;
    }

    UEnemySwordsmanAnimInstance* AnimInstance =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (AnimInstance)
    {
        AnimInstance->StopAttackAnimation();
    }
}

// ==========================================
// DEATH
// ==========================================

void AEnemySwordsmanAI::Die()
{
    if (bIsDead)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(WallCheckTimer);
    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);

    StopCapitolAttack();
    StopWallAttack();
    StopMovement();

    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (MeshComponent)
    {
        UEnemySwordsmanAnimInstance* AnimInstance =
            Cast<UEnemySwordsmanAnimInstance>(
                MeshComponent->GetAnimInstance()
            );

        if (AnimInstance)
        {
            AnimInstance->StopAttackAnimation();
            AnimInstance->bIsDead = true;
        }
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Death animation triggered")
    );

    // Parent handles death notification and removal.
    Super::Die();
}

// ==========================================
// END PLAY
// ==========================================

void AEnemySwordsmanAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    GetWorldTimerManager().ClearTimer(WallCheckTimer);
    GetWorldTimerManager().ClearTimer(WallAttackTimer);
    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    Super::EndPlay(EndPlayReason);
}
