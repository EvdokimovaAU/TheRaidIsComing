
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemySwordsmanAnimInstance.h"
#include "AI/GameBalanceManager.h"
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
// CONSTRUCTOR / BEGIN PLAY
// ==========================================

AEnemySwordsmanAI::AEnemySwordsmanAI()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

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


void AEnemySwordsmanAI::ApplyEnemyBalance()
{
    Super::ApplyEnemyBalance();

    AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this);

    if (!IsValid(Balance))
    {
        return;
    }

    const FSwordsmanBalanceSettings& Settings =
        Balance->Swordsman;

    WallDamage =
        FMath::Max(0.0f, Settings.WallDamage);

    WallAttackCooldown =
        FMath::Max(0.01f, Settings.WallAttackCooldown);

    WallAttackDistance =
        FMath::Max(0.0f, Settings.WallAttackDistance);

    WallDetectionDistance =
        FMath::Max(0.0f, Settings.WallDetectionDistance);

    WallApproachSpeed =
        FMath::Max(0.0f, Settings.WallApproachSpeed);

    UE_LOG(LogTemp, Log,
        TEXT("[Swordsman] Balance applied | WallDamage=%.1f WallRange=%.1f ApproachSpeed=%.1f"),
        WallDamage,
        WallAttackDistance,
        WallApproachSpeed);
}


void AEnemySwordsmanAI::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bApproachingWall && !IsWaitingForMage())
    {
        UpdateWallApproach(DeltaSeconds);
    }
}

// ==========================================
// WAIT FOR MAGE
// ==========================================

void AEnemySwordsmanAI::WaitForMage(AActor* Mage)
{
    if (!IsAlive() || !IsValid(Mage))
        return;

    if (IsWaitingForMage() ||
        IsAttackingWall() ||
        IsAttackingCapitol())
    {
        return;
    }

    WaitingForMage = Mage;

    StopMovement();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[Swordsman] Waiting for mage: %s"),
        *Mage->GetName()
    );
}

void AEnemySwordsmanAI::ResumeAfterMage(AActor* Mage)
{
    if (WaitingForMage.Get() != Mage)
        return;

    WaitingForMage.Reset();

    if (!IsAlive())
        return;

    MoveToTarget();

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[Swordsman] Mage finished. Movement resumed.")
    );
}

bool AEnemySwordsmanAI::IsWaitingForMage() const
{
    return WaitingForMage.IsValid();
}

bool AEnemySwordsmanAI::IsAttackingWall() const
{
    return bAttackingWall ||
        bApproachingWall ||
        bAvoidingWall;
}

bool AEnemySwordsmanAI::IsAttackingCapitol() const
{
    return bAttackingCapitol;
}

void AEnemySwordsmanAI::MoveToTarget()
{
    if (IsWaitingForMage())
        return;

    Super::MoveToTarget();
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

FVector AEnemySwordsmanAI::GetClosestWallPoint(
    const AActor* Wall) const
{
    if (!IsValid(Wall))
        return GetActorLocation();

    const FBox Bounds = Wall->GetComponentsBoundingBox();
    const FVector Position = GetActorLocation();

    if (!Bounds.IsValid)
        return Wall->GetActorLocation();

    return FVector(
        FMath::Clamp(Position.X, Bounds.Min.X, Bounds.Max.X),
        FMath::Clamp(Position.Y, Bounds.Min.Y, Bounds.Max.Y),
        Position.Z
    );
}

float AEnemySwordsmanAI::GetDistanceToWall(
    const AActor* Wall) const
{
    if (!IsValid(Wall))
        return TNumericLimits<float>::Max();

    return FVector::Dist2D(
        GetActorLocation(),
        GetClosestWallPoint(Wall)
    );
}

// ==========================================
// WALL DETECTION
// ==========================================

void AEnemySwordsmanAI::CheckWallAhead()
{
    if (!IsAlive() ||
        IsWaitingForMage() ||
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
    Direction.Z = 0.f;

    if (!Direction.Normalize())
    {
        Direction =
            Capitol->GetActorLocation() - EnemyLocation;
        Direction.Z = 0.f;

        if (!Direction.Normalize())
            return;
    }

    const float Radius =
        FMath::Max(WallDetectionDistance, 100.f);

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
            continue;

        const float Distance = GetDistanceToWall(Actor);

        if (Distance > Radius)
            continue;

        FVector ToWall =
            GetClosestWallPoint(Actor) - EnemyLocation;

        ToWall.Z = 0.f;

        if (ToWall.Normalize() &&
            FVector::DotProduct(Direction, ToWall) < -0.25f)
        {
            continue;
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
// NAVIGATION CHECK
// ==========================================

bool AEnemySwordsmanAI::HasFreePathToCapitol() const
{
    if (!IsValid(Capitol) || !GetWorld())
        return false;

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
        return false;

    FNavLocation Destination;

    if (!NavSys->ProjectPointToNavigation(
        Capitol->GetActorLocation(),
        Destination,
        FVector(250.f, 250.f, 300.f)))
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

void AEnemySwordsmanAI::OnObstacleDetected(
    AActor* Obstacle)
{
    if (!IsAlive() ||
        IsWaitingForMage() ||
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
        FMath::Clamp(WallAttackChance, 0.f, 100.f);

    if (FMath::FRandRange(0.f, 100.f) < Chance)
    {
        StartWallAttack(Obstacle);
    }
    else if (!TryAvoidWall(Obstacle))
    {
        StartWallAttack(Obstacle);
    }
}

// ==========================================
// AVOID WALL
// ==========================================

bool AEnemySwordsmanAI::TryAvoidWall(AActor* Wall)
{
    if (!IsAlive() ||
        IsWaitingForMage() ||
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
        return false;

    const FVector EnemyPosition = GetActorLocation();

    FVector Forward =
        Capitol->GetActorLocation() - EnemyPosition;

    Forward.Z = 0.f;

    if (!Forward.Normalize())
        return false;

    const FVector Right =
        FVector::CrossProduct(
            FVector::UpVector,
            Forward
        ).GetSafeNormal();

    const FVector WallPosition = Wall->GetActorLocation();

    const float Distance =
        FMath::Max(WallAvoidanceDistance, 50.f);

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
            FVector(150.f, 150.f, 250.f)))
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

        float Length = 0.f;

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
        return false;

    StopMovement();

    const EPathFollowingRequestResult::Type Result =
        ControllerAI->MoveToLocation(
            BestPoint,
            75.f,
            true,
            true,
            true,
            false,
            nullptr,
            true
        );

    if (Result == EPathFollowingRequestResult::Failed)
        return false;

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

void AEnemySwordsmanAI::UpdateWallAvoidance()
{
    if (!IsAlive())
    {
        StopWallAvoidance();
        return;
    }

    if (IsWaitingForMage())
        return;

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        AvoidanceDestination
    );

    const float Elapsed =
        GetWorld()->GetTimeSeconds() - AvoidanceStartTime;

    if (Distance <= 100.f)
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
            StartWallAttack(Wall);
        else
            MoveToTarget();
    }
}

void AEnemySwordsmanAI::StopWallAvoidance()
{
    GetWorldTimerManager().ClearTimer(WallAvoidanceTimer);

    bAvoidingWall = false;
    AvoidedWall.Reset();
    AvoidanceDestination = FVector::ZeroVector;
    AvoidanceStartTime = 0.f;
}

// ==========================================
// WALL ATTACK
// ==========================================

void AEnemySwordsmanAI::StartWallAttack(AActor* Wall)
{
    if (!IsAlive() ||
        IsWaitingForMage() ||
        !IsValidWall(Wall))
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

    if (Distance <= WallAttackDistance)
    {
        BeginWallDamage();
    }
    else
    {
        bApproachingWall = true;
        ApproachStartTime = GetWorld()->GetTimeSeconds();
        PreviousWallDistance = Distance;
        StuckTime = 0.f;

        SetActorTickEnabled(true);
    }
}

void AEnemySwordsmanAI::UpdateWallApproach(
    float DeltaSeconds)
{
    if (IsWaitingForMage())
        return;

    ARICBuilding* Wall = TargetWall.Get();

    if (!IsAlive() || !IsValidWall(Wall))
    {
        OnWallDestroyed();
        return;
    }

    const float Distance = GetDistanceToWall(Wall);

    if (Distance <= WallAttackDistance)
    {
        BeginWallDamage();
        return;
    }

    const FVector CurrentLocation = GetActorLocation();
    const FVector TargetPoint = GetClosestWallPoint(Wall);

    FVector Direction = TargetPoint - CurrentLocation;
    Direction.Z = 0.f;

    if (!Direction.Normalize())
    {
        BeginWallDamage();
        return;
    }

    const float Elapsed =
        GetWorld()->GetTimeSeconds() - ApproachStartTime;

    if (Elapsed >= WallApproachTimeout)
    {
        StopWallAttack();
        MoveToTarget();
        return;
    }

    if (Distance < PreviousWallDistance - 0.5f)
        StuckTime = 0.f;
    else
        StuckTime += DeltaSeconds;

    PreviousWallDistance = Distance;

    if (StuckTime >= 1.5f)
    {
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

    if (SavedWalkSpeed <= 0.f)
        SavedWalkSpeed = Movement->MaxWalkSpeed;

    Movement->MaxWalkSpeed =
        FMath::Max(WallApproachSpeed, 50.f);

    AddMovementInput(Direction, 1.f);
    SetActorRotation(Direction.Rotation());
}

void AEnemySwordsmanAI::BeginWallDamage()
{
    if (!bAttackingWall ||
        !IsValidWall(TargetWall.Get()))
    {
        OnWallDestroyed();
        return;
    }

    StopWallApproach();
    StopMovement();

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

void AEnemySwordsmanAI::StopWallApproach()
{
    bApproachingWall = false;
    SetActorTickEnabled(false);

    if (SavedWalkSpeed > 0.f && GetCharacterMovement())
    {
        GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed;
        SavedWalkSpeed = 0.f;
    }
}

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

    if (Distance > WallAttackDistance + 30.f)
    {
        GetWorldTimerManager().ClearTimer(WallAttackTimer);

        bApproachingWall = true;
        ApproachStartTime = GetWorld()->GetTimeSeconds();
        PreviousWallDistance = Distance;
        StuckTime = 0.f;

        SetActorTickEnabled(true);
        return;
    }

    FVector LookDirection =
        Wall->GetActorLocation() - GetActorLocation();

    LookDirection.Z = 0.f;

    if (!LookDirection.IsNearlyZero())
        SetActorRotation(LookDirection.Rotation());

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
        OnWallDestroyed();
}

void AEnemySwordsmanAI::StopWallAttack()
{
    GetWorldTimerManager().ClearTimer(WallAttackTimer);

    StopWallApproach();

    bAttackingWall = false;
    TargetWall = nullptr;

    StopAttackAnimation();
}

void AEnemySwordsmanAI::OnWallDestroyed()
{
    StopWallAttack();

    if (IsAlive() && !IsWaitingForMage())
        MoveToTarget();
}

// ==========================================
// CAPITOL ATTACK
// ==========================================

void AEnemySwordsmanAI::CheckCapitolDistance()
{
    if (!IsAlive() ||
        IsWaitingForMage() ||
        bAttackingCapitol ||
        bAttackingWall ||
        bAvoidingWall)
    {
        return;
    }

    ARICBuilding* Building =
        Cast<ARICBuilding>(Capitol.Get());

    if (!IsValid(Building) || !Building->IsAlive())
        return;

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()
    );

    if (Distance <= AttackRange)
        StartCapitolAttack();
}

void AEnemySwordsmanAI::StartCapitolAttack()
{
    ARICBuilding* Building =
        Cast<ARICBuilding>(Capitol.Get());

    if (IsWaitingForMage() ||
        bAttackingCapitol ||
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
        StopCapitolAttack();
}

void AEnemySwordsmanAI::StopCapitolAttack()
{
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    bAttackingCapitol = false;
    StopAttackAnimation();
}

// ==========================================
// ATTACK ANIMATION
// ==========================================

void AEnemySwordsmanAI::PlayAttackAnimation()
{
    if (!IsAlive())
        return;

    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (!MeshComponent)
        return;

    UEnemySwordsmanAnimInstance* Anim =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (Anim)
        Anim->PlayAttackAnimation();
}

void AEnemySwordsmanAI::StopAttackAnimation()
{
    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (!MeshComponent)
        return;

    UEnemySwordsmanAnimInstance* Anim =
        Cast<UEnemySwordsmanAnimInstance>(
            MeshComponent->GetAnimInstance()
        );

    if (Anim)
        Anim->StopAttackAnimation();
}

// ==========================================
// DEATH / END PLAY
// ==========================================

void AEnemySwordsmanAI::Die()
{
    if (bIsDead)
        return;

    WaitingForMage.Reset();

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

void AEnemySwordsmanAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    WaitingForMage.Reset();

    GetWorldTimerManager().ClearTimer(WallCheckTimer);
    GetWorldTimerManager().ClearTimer(WallAttackTimer);
    GetWorldTimerManager().ClearTimer(WallAvoidanceTimer);
    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    Super::EndPlay(EndPlayReason);
}
