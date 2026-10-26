
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemySwordsmanAnimInstance.h"
#include "Navigation/PathFollowingComponent.h"
#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

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
        bAvoidingWall ||
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
    if (!IsAlive() ||
        !IsValidWall(Obstacle) ||
        bAttackingWall ||
        bAvoidingWall)
    {
        return;
    }

    const float RandomValue =
        FMath::FRandRange(0.0f, 100.0f);

    const float AttackProbability =
        FMath::Clamp(WallAttackChance, 0.0f, 100.0f);

    if (RandomValue < AttackProbability)
    {
        // ==================================
        // ATTACK WALL
        // ==================================

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Decision: ATTACK | Wall: %s | Roll: %.1f"),
            *Obstacle->GetName(),
            RandomValue
        );

        StopCapitolAttack();

        StartWallAttack(Obstacle);
    }
    else
    {
        // ==================================
        // AVOID WALL
        // ==================================

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Decision: AVOID | Wall: %s | Roll: %.1f"),
            *Obstacle->GetName(),
            RandomValue
        );

        if (!TryAvoidWall(Obstacle))
        {
            // No valid avoidance route was found.
            // Fall back to attacking the obstacle.

            UE_LOG(
                LogTemp,
                Warning,
                TEXT("[Swordsman] Avoidance failed. Attacking wall.")
            );

            StartWallAttack(Obstacle);
        }
    }
}

// ==========================================
// TRY AVOID WALL
// ==========================================

bool AEnemySwordsmanAI::TryAvoidWall(AActor* Wall)
{
    if (!IsAlive() ||
        !IsValidWall(Wall) ||
        !IsValid(Capitol))
    {
        return false;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!EnemyController || !NavSys)
    {
        return false;
    }

    const FVector EnemyLocation = GetActorLocation();
    const FVector WallLocation = Wall->GetActorLocation();
    const FVector CapitolLocation = Capitol->GetActorLocation();

    FVector Forward =
        CapitolLocation - EnemyLocation;

    Forward.Z = 0.0f;

    if (!Forward.Normalize())
    {
        return false;
    }

    const FVector Right =
        FVector::CrossProduct(
            FVector::UpVector,
            Forward
        ).GetSafeNormal();

    const float Distance =
        FMath::Max(WallAvoidanceDistance, 50.0f);

    // Search for points on both sides of the wall,
    // slightly beyond its center toward the Capitol.

    const FVector ForwardOffset =
        Forward * Distance * 0.5f;

    const FVector LeftCandidate =
        WallLocation - Right * Distance + ForwardOffset;

    const FVector RightCandidate =
        WallLocation + Right * Distance + ForwardOffset;

    const FVector Candidates[2] =
    {
        LeftCandidate,
        RightCandidate
    };

    bool bFoundDestination = false;

    FVector BestDestination = FVector::ZeroVector;

    float BestScore = TNumericLimits<float>::Max();

    for (const FVector& Candidate : Candidates)
    {
        FNavLocation NavLocation;

        const bool bProjected =
            NavSys->ProjectPointToNavigation(
                Candidate,
                NavLocation,
                FVector(150.0f, 150.0f, 250.0f)
            );

        if (!bProjected)
        {
            continue;
        }

        UNavigationPath* Path =
            NavSys->FindPathToLocationSynchronously(
                GetWorld(),
                EnemyLocation,
                NavLocation.Location,
                this
            );

        if (!IsValid(Path) ||
            !Path->IsValid() ||
            Path->IsPartial())
        {
            continue;
        }

        const TArray<FVector>& Points =
            Path->PathPoints;

        if (Points.Num() < 2)
        {
            continue;
        }

        float PathLength = 0.0f;

        for (int32 Index = 1; Index < Points.Num(); ++Index)
        {
            PathLength += FVector::Dist(
                Points[Index - 1],
                Points[Index]
            );
        }

        // Prefer a shorter path to the waypoint,
        // with a small preference for proximity
        // to the final objective.

        const float Score =
            PathLength +
            FVector::Dist2D(
                NavLocation.Location,
                CapitolLocation
            ) * 0.2f;

        if (Score < BestScore)
        {
            BestScore = Score;
            BestDestination = NavLocation.Location;
            bFoundDestination = true;
        }
    }

    if (!bFoundDestination)
    {
        return false;
    }

    StopCapitolAttack();
    StopMovement();

    const EPathFollowingRequestResult::Type MoveResult =
        EnemyController->MoveToLocation(
            BestDestination,
            75.0f,
            true,
            true,
            true,
            false,
            nullptr,
            true
        );

    if (MoveResult ==
        EPathFollowingRequestResult::Type::Failed)
    {
        return false;
    }

    bAvoidingWall = true;

    AvoidedWall = Cast<ARICBuilding>(Wall);

    AvoidanceDestination = BestDestination;

    AvoidanceStartTime =
        GetWorld()->GetTimeSeconds();

    GetWorldTimerManager().SetTimer(
        WallAvoidanceTimer,
        this,
        &AEnemySwordsmanAI::UpdateWallAvoidance,
        0.2f,
        true
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Avoiding wall: %s | Destination: %s"),
        *Wall->GetName(),
        *BestDestination.ToString()
    );

    return true;
}

// ==========================================
// UPDATE WALL AVOIDANCE
// ==========================================

void AEnemySwordsmanAI::UpdateWallAvoidance()
{
    if (!IsAlive())
    {
        StopWallAvoidance();
        return;
    }

    const float Distance =
        FVector::Dist2D(
            GetActorLocation(),
            AvoidanceDestination
        );

    const float ElapsedTime =
        GetWorld()->GetTimeSeconds() -
        AvoidanceStartTime;

    if (Distance <= 100.0f)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Avoidance waypoint reached")
        );

        StopWallAvoidance();
        MoveToTarget();
        return;
    }

    if (ElapsedTime >=
        FMath::Max(WallAvoidanceTimeout, 0.5f))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Avoidance timed out")
        );

        ARICBuilding* Wall = AvoidedWall.Get();

        StopWallAvoidance();

        // If still blocked, attack the wall rather
        // than remaining stuck indefinitely.

        if (IsValidWall(Wall))
        {
            const float WallDistance =
                FVector::Dist2D(
                    GetActorLocation(),
                    Wall->GetActorLocation()
                );

            if (WallDistance <=
                FMath::Max(
                    AttackRange,
                    WallDetectionDistance
                ))
            {
                StartWallAttack(Wall);
                return;
            }
        }

        MoveToTarget();
    }
}

// ==========================================
// STOP WALL AVOIDANCE
// ==========================================

void AEnemySwordsmanAI::StopWallAvoidance()
{
    GetWorldTimerManager().ClearTimer(
        WallAvoidanceTimer
    );

    bAvoidingWall = false;

    AvoidedWall.Reset();

    AvoidanceDestination = FVector::ZeroVector;

    AvoidanceStartTime = 0.0f;
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

    StopWallAvoidance();
    StopWallAttack();
    StopCapitolAttack();

    TargetWall = Cast<ARICBuilding>(Wall);
    bAttackingWall = true;

    StopMovement();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall attack started: %s"),
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

    const float Distance =
        FVector::Dist2D(
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

    FDamageEvent DamageEvent(
        UDamageType::StaticClass()
    );

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
    GetWorldTimerManager().ClearTimer(
        WallAttackTimer
    );

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

    const float Distance =
        FVector::Dist2D(
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

    StopWallAvoidance();

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

    const float Distance =
        FVector::Dist2D(
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

    FDamageEvent DamageEvent(
        UDamageType::StaticClass()
    );

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
    GetWorldTimerManager().ClearTimer(
        CapitolAttackTimer
    );

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

    GetWorldTimerManager().ClearTimer(
        WallCheckTimer
    );

    GetWorldTimerManager().ClearTimer(
        CapitolCheckTimer
    );

    StopWallAvoidance();
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
    GetWorldTimerManager().ClearTimer(
        WallCheckTimer
    );

    GetWorldTimerManager().ClearTimer(
        WallAttackTimer
    );

    GetWorldTimerManager().ClearTimer(
        WallAvoidanceTimer
    );

    GetWorldTimerManager().ClearTimer(
        CapitolCheckTimer
    );

    GetWorldTimerManager().ClearTimer(
        CapitolAttackTimer
    );

    Super::EndPlay(EndPlayReason);
}
