
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemySwordsmanAnimInstance.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemySwordsmanAI::AEnemySwordsmanAI()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemySwordsmanAI::BeginPlay()
{
    Super::BeginPlay();

    SetActorTickEnabled(false);

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
// TICK
// ==========================================

void AEnemySwordsmanAI::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bApproachingWall)
    {
        UpdateWallApproach(DeltaSeconds);
    }
}

// ==========================================
// WALL VALIDATION
// ==========================================

bool AEnemySwordsmanAI::IsValidWall(AActor* Actor) const
{
    const ARICBuilding* Wall = Cast<ARICBuilding>(Actor);

    return IsValid(Wall) &&
        Wall->BuildingType == ERICBuildingType::Barrier &&
        Wall->IsAlive();
}

// ==========================================
// CLOSEST POINT ON WALL
// ==========================================

FVector AEnemySwordsmanAI::GetClosestWallPoint(
    const AActor* Wall
) const
{
    if (!IsValid(Wall))
    {
        return GetActorLocation();
    }

    const FBox Bounds = Wall->GetComponentsBoundingBox();
    const FVector Position = GetActorLocation();

    if (!Bounds.IsValid)
    {
        return Wall->GetActorLocation();
    }

    return FVector(
        FMath::Clamp(Position.X, Bounds.Min.X, Bounds.Max.X),
        FMath::Clamp(Position.Y, Bounds.Min.Y, Bounds.Max.Y),
        Position.Z
    );
}

float AEnemySwordsmanAI::GetDistanceToWall(
    const AActor* Wall
) const
{
    if (!IsValid(Wall))
    {
        return TNumericLimits<float>::Max();
    }

    return FVector::Dist2D(
        GetActorLocation(),
        GetClosestWallPoint(Wall)
    );
}

// ==========================================
// DETECT NEARBY WALLS
// ==========================================

void AEnemySwordsmanAI::CheckWallAhead()
{
    if (!IsAlive() ||
        bAttackingWall ||
        bApproachingWall ||
        bAttackingCapitol ||
        bAvoidingWall ||
        !IsValid(Capitol))
    {
        return;
    }

    const FVector EnemyLocation = GetActorLocation();

    FVector Direction = GetVelocity();
    Direction.Z = 0.0f;

    if (!Direction.Normalize())
    {
        Direction = Capitol->GetActorLocation() - EnemyLocation;
        Direction.Z = 0.0f;

        if (!Direction.Normalize())
        {
            return;
        }
    }

    const float Radius =
        FMath::Max(WallDetectionDistance, 100.0f);

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    TArray<FOverlapResult> Results;

    GetWorld()->OverlapMultiByObjectType(
        Results,
        EnemyLocation,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(Radius),
        QueryParams
    );

    AActor* ClosestWall = nullptr;
    float ClosestDistance = TNumericLimits<float>::Max();

    for (const FOverlapResult& Result : Results)
    {
        AActor* Actor = Result.GetActor();

        if (!IsValidWall(Actor))
        {
            continue;
        }

        const float Distance = GetDistanceToWall(Actor);

        if (Distance > Radius)
        {
            continue;
        }

        FVector ToWall =
            GetClosestWallPoint(Actor) - EnemyLocation;

        ToWall.Z = 0.0f;

        if (ToWall.Normalize())
        {
            if (FVector::DotProduct(Direction, ToWall) < -0.25f)
            {
                continue;
            }
        }

        if (Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestWall = Actor;
        }
    }

    if (IsValid(ClosestWall))
    {
        OnObstacleDetected(ClosestWall);
    }
}

// ==========================================
// CHECK NAVIGATION ROUTE
// ==========================================

bool AEnemySwordsmanAI::HasFreePathToCapitol() const
{
    if (!IsValid(Capitol) || !GetWorld())
    {
        return false;
    }

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
    {
        return false;
    }

    FNavLocation Destination;

    if (!NavSys->ProjectPointToNavigation(
        Capitol->GetActorLocation(),
        Destination,
        FVector(250.0f, 250.0f, 300.0f)
    ))
    {
        return false;
    }

    UNavigationPath* Path =
        NavSys->FindPathToLocationSynchronously(
            GetWorld(),
            GetActorLocation(),
            Destination.Location,
            const_cast<AEnemySwordsmanAI*>(this)
        );

    return IsValid(Path) &&
        Path->IsValid() &&
        !Path->IsPartial() &&
        Path->PathPoints.Num() >= 2;
}

// ==========================================
// WALL DECISION
// ==========================================

void AEnemySwordsmanAI::OnObstacleDetected(AActor* Obstacle)
{
    if (!IsAlive() ||
        !IsValidWall(Obstacle) ||
        bAttackingWall ||
        bApproachingWall ||
        bAvoidingWall ||
        bAttackingCapitol)
    {
        return;
    }

    if (!HasFreePathToCapitol())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] No route. Must destroy wall.")
        );

        StartWallAttack(Obstacle);
        return;
    }

    const float Chance =
        FMath::Clamp(WallAttackChance, 0.0f, 100.0f);

    const float Roll =
        FMath::FRandRange(0.0f, 100.0f);

    if (Roll < Chance)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Chose wall attack")
        );

        StartWallAttack(Obstacle);
    }
    else
    {
        if (!TryAvoidWall(Obstacle))
        {
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

    AAIController* ControllerAI =
        Cast<AAIController>(GetController());

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!ControllerAI || !NavSys)
    {
        return false;
    }

    const FVector EnemyPosition = GetActorLocation();

    FVector Forward =
        Capitol->GetActorLocation() - EnemyPosition;

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

    const FVector WallPosition = Wall->GetActorLocation();

    const float Distance =
        FMath::Max(WallAvoidanceDistance, 50.0f);

    const FVector Candidates[2] =
    {
        WallPosition + Right * Distance +
            Forward * Distance * 0.5f,

        WallPosition - Right * Distance +
            Forward * Distance * 0.5f
    };

    bool bFound = false;
    FVector BestPoint = FVector::ZeroVector;
    float BestScore = TNumericLimits<float>::Max();

    for (const FVector& Candidate : Candidates)
    {
        FNavLocation Projected;

        if (!NavSys->ProjectPointToNavigation(
            Candidate,
            Projected,
            FVector(150.0f, 150.0f, 250.0f)
        ))
        {
            continue;
        }

        UNavigationPath* Path =
            NavSys->FindPathToLocationSynchronously(
                GetWorld(),
                EnemyPosition,
                Projected.Location,
                this
            );

        if (!IsValid(Path) ||
            !Path->IsValid() ||
            Path->IsPartial() ||
            Path->PathPoints.Num() < 2)
        {
            continue;
        }

        float Length = 0.0f;

        for (int32 i = 1; i < Path->PathPoints.Num(); ++i)
        {
            Length += FVector::Dist(
                Path->PathPoints[i - 1],
                Path->PathPoints[i]
            );
        }

        const float Score =
            Length +
            FVector::Dist2D(
                Projected.Location,
                Capitol->GetActorLocation()
            ) * 0.2f;

        if (Score < BestScore)
        {
            BestScore = Score;
            BestPoint = Projected.Location;
            bFound = true;
        }
    }

    if (!bFound)
    {
        return false;
    }

    StopMovement();

    const EPathFollowingRequestResult::Type Result =
        ControllerAI->MoveToLocation(
            BestPoint,
            75.0f,
            true,
            true,
            true,
            false,
            nullptr,
            true
        );

    if (Result == EPathFollowingRequestResult::Failed)
    {
        return false;
    }

    bAvoidingWall = true;
    AvoidedWall = Cast<ARICBuilding>(Wall);
    AvoidanceDestination = BestPoint;
    AvoidanceStartTime = GetWorld()->GetTimeSeconds();

    GetWorldTimerManager().SetTimer(
        WallAvoidanceTimer,
        this,
        &AEnemySwordsmanAI::UpdateWallAvoidance,
        0.2f,
        true
    );

    return true;
}

// ==========================================
// UPDATE AVOIDANCE
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

    const float Elapsed =
        GetWorld()->GetTimeSeconds() - AvoidanceStartTime;

    if (Distance <= 100.0f)
    {
        StopWallAvoidance();
        MoveToTarget();
        return;
    }

    if (Elapsed >= FMath::Max(WallAvoidanceTimeout, 0.5f))
    {
        ARICBuilding* Wall = AvoidedWall.Get();

        StopWallAvoidance();

        if (IsValidWall(Wall))
        {
            StartWallAttack(Wall);
        }
        else
        {
            MoveToTarget();
        }
    }
}

void AEnemySwordsmanAI::StopWallAvoidance()
{
    GetWorldTimerManager().ClearTimer(WallAvoidanceTimer);

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

    StopWallAvoidance();
    StopWallAttack();
    StopCapitolAttack();

    TargetWall = Cast<ARICBuilding>(Wall);
    bAttackingWall = true;

    StopMovement();

    const float Distance = GetDistanceToWall(Wall);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall attack START | Distance: %.1f"),
        Distance
    );

    if (Distance <= WallAttackDistance)
    {
        BeginWallDamage();
    }
    else
    {
        bApproachingWall = true;
        ApproachStartTime = GetWorld()->GetTimeSeconds();
        PreviousWallDistance = Distance;
        StuckTime = 0.0f;

        SetActorTickEnabled(true);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] LOCAL wall approach started")
        );
    }
}

// ==========================================
// LOCAL MOVEMENT TOWARD WALL
// ==========================================

void AEnemySwordsmanAI::UpdateWallApproach(
    float DeltaSeconds
)
{
    ARICBuilding* Wall = TargetWall.Get();

    if (!IsAlive() || !IsValidWall(Wall))
    {
        OnWallDestroyed();
        return;
    }

    const float Distance = GetDistanceToWall(Wall);

    if (Distance <= WallAttackDistance)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Wall reached | Distance: %.1f"),
            Distance
        );

        BeginWallDamage();
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector TargetPoint = GetClosestWallPoint(Wall);

    FVector Direction = TargetPoint - CurrentLocation;
    Direction.Z = 0.0f;

    if (!Direction.Normalize())
    {
        BeginWallDamage();
        return;
    }

    const float Elapsed =
        GetWorld()->GetTimeSeconds() - ApproachStartTime;

    if (Elapsed >= WallApproachTimeout)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Wall approach timeout | Distance: %.1f"),
            Distance
        );

        StopWallAttack();
        MoveToTarget();
        return;
    }

    if (Distance < PreviousWallDistance - 0.5f)
    {
        StuckTime = 0.0f;
    }
    else
    {
        StuckTime += DeltaSeconds;
    }

    PreviousWallDistance = Distance;

    if (StuckTime >= 1.5f)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Swordsman] Wall approach stuck | Distance: %.1f"),
            Distance
        );

        StopWallAttack();
        MoveToTarget();
        return;
    }

    UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    if (!Movement)
    {
        StopWallAttack();
        return;
    }

    // Save normal movement speed before local approach.
    if (SavedWalkSpeed <= 0.0f)
    {
        SavedWalkSpeed = Movement->MaxWalkSpeed;
    }

    Movement->MaxWalkSpeed =
        FMath::Max(WallApproachSpeed, 50.0f);

    AddMovementInput(Direction, 1.0f);
    SetActorRotation(Direction.Rotation());
}

// ==========================================
// BEGIN DEALING WALL DAMAGE
// ==========================================

void AEnemySwordsmanAI::BeginWallDamage()
{
    if (!bAttackingWall || !IsValidWall(TargetWall.Get()))
    {
        OnWallDestroyed();
        return;
    }

    StopWallApproach();
    StopMovement();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Wall in attack range. Starting hits.")
    );

    AttackWall();

    if (bAttackingWall)
    {
        GetWorldTimerManager().SetTimer(
            WallAttackTimer,
            this,
            &AEnemySwordsmanAI::AttackWall,
            FMath::Max(WallAttackCooldown, 0.1f),
            true
        );
    }
}

// ==========================================
// STOP WALL APPROACH
// ==========================================

void AEnemySwordsmanAI::StopWallApproach()
{
    bApproachingWall = false;
    SetActorTickEnabled(false);

    if (SavedWalkSpeed > 0.0f && GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed;
        SavedWalkSpeed = 0.0f;
    }
}

// ==========================================
// WALL DAMAGE
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

    const float Distance = GetDistanceToWall(Wall);

    if (Distance > WallAttackDistance + 30.0f)
    {
        GetWorldTimerManager().ClearTimer(WallAttackTimer);

        bApproachingWall = true;
        ApproachStartTime = GetWorld()->GetTimeSeconds();
        PreviousWallDistance = Distance;
        StuckTime = 0.0f;

        SetActorTickEnabled(true);
        return;
    }

    FVector LookDirection =
        Wall->GetActorLocation() - GetActorLocation();

    LookDirection.Z = 0.0f;

    if (!LookDirection.IsNearlyZero())
    {
        SetActorRotation(LookDirection.Rotation());
    }

    StopMovement();
    PlayAttackAnimation();

    FDamageEvent DamageEvent(
        UDamageType::StaticClass()
    );

    const float AppliedDamage = Wall->TakeDamage(
        WallDamage,
        DamageEvent,
        GetController(),
        this
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] WALL HIT | Damage: %.1f | Wall: %s"),
        AppliedDamage,
        *GetNameSafe(Wall)
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

    StopWallApproach();

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
        bAttackingWall ||
        bAvoidingWall)
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

    GetWorldTimerManager().SetTimer(
        CapitolAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackCapitol,
        FMath::Max(AttackCooldown, 0.1f),
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

    const float AppliedDamage = Building->TakeDamage(
        Damage,
        DamageEvent,
        GetController(),
        this
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Swordsman] Capitol hit | Damage: %.1f"),
        AppliedDamage
    );

    if (!IsValid(Building) || !Building->IsAlive())
    {
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

    UEnemySwordsmanAnimInstance* Anim =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (Anim)
    {
        Anim->PlayAttackAnimation();
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

    UEnemySwordsmanAnimInstance* Anim =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (Anim)
    {
        Anim->StopAttackAnimation();
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

    StopWallAvoidance();
    StopCapitolAttack();
    StopWallAttack();
    StopMovement();

    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (MeshComponent)
    {
        UEnemySwordsmanAnimInstance* Anim =
            Cast<UEnemySwordsmanAnimInstance>(
                MeshComponent->GetAnimInstance()
            );

        if (Anim)
        {
            Anim->StopAttackAnimation();
            Anim->bIsDead = true;
        }
    }

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
    GetWorldTimerManager().ClearTimer(WallAvoidanceTimer);
    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    Super::EndPlay(EndPlayReason);
}
