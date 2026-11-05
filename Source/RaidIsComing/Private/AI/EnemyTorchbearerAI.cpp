
#include "AI/EnemyTorchbearerAI.h"
#include "AI/BurningComponent.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICBuildTypes.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemyTorchbearerAI::AEnemyTorchbearerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    bAutoStartMovement = true;
    MovementSpeed = 300.f;
    AcceptanceRadius = 120.f;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemyTorchbearerAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        BehaviorTimer,
        this,
        &AEnemyTorchbearerAI::UpdateBehavior,
        FMath::Max(CheckInterval, 0.05f),
        true
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[Torchbearer] Initialized: %s"),
        *GetName());
}

// ==========================================
// BURNING COMPONENT
// ==========================================

UBurningComponent*
AEnemyTorchbearerAI::GetBurningComponent(
    AActor* Actor) const
{
    return IsValid(Actor)
        ? Actor->FindComponentByClass<UBurningComponent>()
        : nullptr;
}

// ==========================================
// DISTANCE TO BUILDING BOUNDS
// ==========================================

float AEnemyTorchbearerAI::GetDistanceToBuilding(
    const ARICBuilding* Building) const
{
    if (!IsValid(Building))
    {
        return TNumericLimits<float>::Max();
    }

    const FVector MyLocation = GetActorLocation();

    const FBox Bounds =
        Building->GetComponentsBoundingBox();

    if (!Bounds.IsValid)
    {
        return FVector::Dist2D(
            MyLocation,
            Building->GetActorLocation()
        );
    }

    const FVector ClosestPoint(
        FMath::Clamp(
            MyLocation.X,
            Bounds.Min.X,
            Bounds.Max.X
        ),
        FMath::Clamp(
            MyLocation.Y,
            Bounds.Min.Y,
            Bounds.Max.Y
        ),
        MyLocation.Z
    );

    return FVector::Dist2D(
        MyLocation,
        ClosestPoint
    );
}

// ==========================================
// FIND WALL AHEAD
// ==========================================

ARICBuilding*
AEnemyTorchbearerAI::FindWallAhead() const
{
    if (!GetWorld() || !IsValid(Capitol))
        return nullptr;

    const FVector Start = GetActorLocation();

    FVector Direction = GetVelocity();
    Direction.Z = 0.f;

    if (!Direction.Normalize())
    {
        Direction =
            Capitol->GetActorLocation() - Start;

        Direction.Z = 0.f;

        if (!Direction.Normalize())
            return nullptr;
    }

    ARICBuilding* BestWall = nullptr;

    float BestDistance =
        TNumericLimits<float>::Max();

    for (TActorIterator<ARICBuilding> It(
        GetWorld()); It; ++It)
    {
        ARICBuilding* Building = *It;

        if (!IsValid(Building) ||
            !Building->IsAlive() ||
            !Building->IsDestructible() ||
            Building->BuildingType !=
            ERICBuildingType::Barrier)
        {
            continue;
        }

        if (!GetBurningComponent(Building))
            continue;

        const FBox Bounds =
            Building->GetComponentsBoundingBox();

        const FVector Closest(
            FMath::Clamp(
                Start.X,
                Bounds.Min.X,
                Bounds.Max.X
            ),
            FMath::Clamp(
                Start.Y,
                Bounds.Min.Y,
                Bounds.Max.Y
            ),
            Start.Z
        );

        FVector Offset = Closest - Start;
        Offset.Z = 0.f;

        const float Distance = Offset.Size();

        if (Distance > WallDetectionDistance ||
            Distance >= BestDistance)
        {
            continue;
        }

        if (!Offset.IsNearlyZero() &&
            FVector::DotProduct(
                Direction,
                Offset.GetSafeNormal()
            ) < 0.25f)
        {
            continue;
        }

        BestDistance = Distance;
        BestWall = Building;
    }

    return BestWall;
}

// ==========================================
// UPDATE BEHAVIOR
// ==========================================

void AEnemyTorchbearerAI::UpdateBehavior()
{
    if (!IsAlive() || bPreparingIgnition)
        return;

    // If the current target exists, handle it.
    if (ARICBuilding* Building = FireTarget.Get())
    {
        if (!Building->IsAlive())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Torchbearer] Target destroyed: %s"),
                *GetNameSafe(Building));

            FireTarget.Reset();
            bHasIgnitedTarget = false;

            ResumeMovement();
            return;
        }

        UBurningComponent* Burning =
            GetBurningComponent(Building);

        if (!Burning)
        {
            FireTarget.Reset();
            bHasIgnitedTarget = false;

            ResumeMovement();
            return;
        }

        // Wait until the current fire ends.
        if (Burning->IsBurning())
        {
            return;
        }

        // Pause after the previous ignition.
        if (GetWorld()->GetTimeSeconds() <
            NextIgnitionAllowedTime)
        {
            return;
        }

        // Start another ignition.
        StartIgnition(Building);
        return;
    }

    // --------------------------------------
    // CAPITOL
    // --------------------------------------

    ARICBuilding* CapitolBuilding =
        Cast<ARICBuilding>(Capitol.Get());

    if (IsValid(CapitolBuilding) &&
        CapitolBuilding->IsAlive())
    {
        const float Distance =
            GetDistanceToBuilding(CapitolBuilding);

        if (Distance <= FireRange)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Torchbearer] Capitol reached. "
                    "Distance: %.1f"),
                Distance);

            StartIgnition(CapitolBuilding);
            return;
        }
    }

    // --------------------------------------
    // WALL
    // --------------------------------------

    if (ARICBuilding* Wall = FindWallAhead())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Torchbearer] Wall detected: %s"),
            *GetNameSafe(Wall));

        StartIgnition(Wall);
        return;
    }
}

// ==========================================
// START IGNITION
// ==========================================

void AEnemyTorchbearerAI::StartIgnition(
    ARICBuilding* Building)
{
    if (!IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive() ||
        bPreparingIgnition)
    {
        return;
    }

    UBurningComponent* Burning =
        GetBurningComponent(Building);

    if (!Burning)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Torchbearer] Missing BurningComponent: %s"),
            *GetNameSafe(Building));

        return;
    }

    FireTarget = Building;

    StopMovement();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
    }

    if (Burning->IsBurning())
    {
        return;
    }

    bPreparingIgnition = true;

    FVector Direction =
        Building->GetActorLocation() -
        GetActorLocation();

    Direction.Z = 0.f;

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    // Play ignition montage.
    if (IgniteMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(IgniteMontage);
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Torchbearer] Preparing ignition: %s"),
        *GetNameSafe(Building));

    GetWorldTimerManager().SetTimer(
        IgniteTimer,
        this,
        &AEnemyTorchbearerAI::CompleteIgnition,
        FMath::Max(0.01f, IgniteDelay),
        false
    );
}

// ==========================================
// COMPLETE IGNITION
// ==========================================

void AEnemyTorchbearerAI::CompleteIgnition()
{
    bPreparingIgnition = false;

    if (!IsAlive())
        return;

    ARICBuilding* Building = FireTarget.Get();

    if (!IsValid(Building) || !Building->IsAlive())
    {
        FireTarget.Reset();
        bHasIgnitedTarget = false;

        ResumeMovement();
        return;
    }

    const float Distance =
        GetDistanceToBuilding(Building);

    const bool bIsWall =
        Building->BuildingType ==
        ERICBuildingType::Barrier;

    const float AllowedDistance = bIsWall
        ? WallDetectionDistance
        : FireRange;

    if (Distance > AllowedDistance + 50.f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Torchbearer] Ignition cancelled. "
                "Target: %s | Distance: %.1f"),
            *GetNameSafe(Building),
            Distance);

        FireTarget.Reset();
        bHasIgnitedTarget = false;

        ResumeMovement();
        return;
    }

    UBurningComponent* Burning =
        GetBurningComponent(Building);

    if (!Burning)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Torchbearer] BurningComponent missing: %s"),
            *GetNameSafe(Building));

        FireTarget.Reset();
        bHasIgnitedTarget = false;

        ResumeMovement();
        return;
    }

    const bool bIgnited = Burning->Ignite(this);

    UE_LOG(LogTemp, Warning,
        TEXT("[Torchbearer] Ignite %s | "
            "Target: %s | Distance: %.1f"),
        bIgnited ? TEXT("SUCCESS") : TEXT("FAILED"),
        *GetNameSafe(Building),
        Distance);

    if (bIgnited)
    {
        bHasIgnitedTarget = true;

        NextIgnitionAllowedTime =
            GetWorld()->GetTimeSeconds() +
            FMath::Max(0.f, Burning->BurnDuration) +
            FMath::Max(0.f, ReigniteDelay);
    }
    else if (!Burning->IsBurning())
    {
        // Avoid retrying every behavior update.
        NextIgnitionAllowedTime =
            GetWorld()->GetTimeSeconds() +
            FMath::Max(0.1f, ReigniteDelay);
    }
}

// ==========================================
// RESUME MOVEMENT
// ==========================================

void AEnemyTorchbearerAI::ResumeMovement()
{
    if (!IsAlive())
        return;

    if (bPreparingIgnition ||
        FireTarget.IsValid())
    {
        return;
    }

    MoveToTarget();
}

// ==========================================
// CANCEL IGNITION
// ==========================================

void AEnemyTorchbearerAI::CancelIgnition()
{
    GetWorldTimerManager().ClearTimer(
        IgniteTimer
    );

    bPreparingIgnition = false;
    bHasIgnitedTarget = false;

    FireTarget.Reset();

    if (IgniteMontage && GetMesh())
    {
        if (UAnimInstance* Anim =
            GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Stop(
                0.1f,
                IgniteMontage
            );
        }
    }
}

// ==========================================
// MOVE TO TARGET
// ==========================================

void AEnemyTorchbearerAI::MoveToTarget()
{
    if (!IsAlive() ||
        bPreparingIgnition ||
        FireTarget.IsValid())
    {
        return;
    }

    Super::MoveToTarget();
}

// ==========================================
// DIE
// ==========================================

void AEnemyTorchbearerAI::Die()
{
    if (!IsAlive())
        return;

    GetWorldTimerManager().ClearTimer(
        BehaviorTimer
    );

    CancelIgnition();

    // Existing fire continues after death.
    Super::Die();
}

// ==========================================
// END PLAY
// ==========================================

void AEnemyTorchbearerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        BehaviorTimer
    );

    CancelIgnition();

    Super::EndPlay(EndPlayReason);
}
