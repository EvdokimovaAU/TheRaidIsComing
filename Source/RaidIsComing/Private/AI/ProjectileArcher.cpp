
#include "AI/ProjectileArcher.h"

#include "AI/AIBase.h"
#include "AI/EnemyAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/GameBalanceManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    AEnemyBarrier* FindProtectingBarrier(
        const AEnemyAI* Enemy)
    {
        if (!IsValid(Enemy) || !Enemy->GetWorld())
        {
            return nullptr;
        }

        AEnemyBarrier* ClosestBarrier = nullptr;
        float ClosestDistanceSq = TNumericLimits<float>::Max();

        for (TActorIterator<AEnemyBarrier> It(Enemy->GetWorld());
            It; ++It)
        {
            AEnemyBarrier* Barrier = *It;

            if (!IsValid(Barrier) ||
                !Barrier->IsBarrierActive() ||
                !Barrier->ContainsLocation(
                    Enemy->GetActorLocation()))
            {
                continue;
            }

            const float DistanceSq = FVector::DistSquared(
                Barrier->GetActorLocation(),
                Enemy->GetActorLocation()
            );

            if (DistanceSq < ClosestDistanceSq)
            {
                ClosestDistanceSq = DistanceSq;
                ClosestBarrier = Barrier;
            }
        }

        return ClosestBarrier;
    }

    float GetBarrierDamageMultiplier(
        const UObject* WorldContext)
    {
        const AGameBalanceManager* Balance =
            AGameBalanceManager::GetBalanceManager(WorldContext);

        if (!IsValid(Balance))
        {
            return 0.5f;
        }

        return FMath::Clamp(
            Balance->General.BarrierDamageMultiplier,
            0.0f,
            1.0f
        );
    }
}

// ==========================================
// CONSTRUCTOR
// ==========================================

AProjectileArcher::AProjectileArcher()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );

    RootComponent = SceneRoot;

    // Projectile collisions are handled in code.
    SetActorEnableCollision(false);

    InitialLifeSpan = 10.0f;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AProjectileArcher::BeginPlay()
{
    Super::BeginPlay();

    StartLocation = GetActorLocation();
}

// ==========================================
// INITIALIZATION
// ==========================================

void AProjectileArcher::InitializeProjectile(
    AAIBase* NewTarget,
    float NewDamage)
{
    if (!IsValid(NewTarget) || !NewTarget->IsAlive())
    {
        Destroy();
        return;
    }

    TargetActor = NewTarget;

    ProjectileDamage = FMath::Max(0.0f, NewDamage);

    StartLocation = GetActorLocation();

    const float Distance = FVector::Dist(
        StartLocation,
        NewTarget->GetActorLocation()
    );

    FlightDuration = FMath::Max(
        Distance / FMath::Max(FlightSpeed, 1.0f),
        0.05f
    );

    ElapsedTime = 0.0f;
    bInitialized = true;
}

// ==========================================
// MOVEMENT
// ==========================================

void AProjectileArcher::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bInitialized)
    {
        return;
    }

    if (!TargetActor.IsValid() ||
        !TargetActor->IsAlive())
    {
        Destroy();
        return;
    }

    ElapsedTime += DeltaTime;

    const float Alpha = FMath::Clamp(
        ElapsedTime / FlightDuration,
        0.0f,
        1.0f
    );

    const FVector EndLocation =
        TargetActor->GetActorLocation();

    FVector NewLocation = FMath::Lerp(
        StartLocation,
        EndLocation,
        Alpha
    );

    const float Height =
        4.0f * ArcHeight * Alpha * (1.0f - Alpha);

    NewLocation.Z += Height;

    const FVector Direction =
        NewLocation - GetActorLocation();

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    SetActorLocation(NewLocation);

    const float DistanceToTarget = FVector::Dist(
        NewLocation,
        EndLocation
    );

    if (DistanceToTarget <= HitRadius || Alpha >= 1.0f)
    {
        HitTarget();
    }
}

// ==========================================
// DAMAGE
// ==========================================

void AProjectileArcher::HitTarget()
{
    AAIBase* Target = TargetActor.Get();

    if (!IsValid(Target) || !Target->IsAlive())
    {
        Destroy();
        return;
    }

    AEnemyAI* Enemy = Cast<AEnemyAI>(Target);

    if (IsValid(Enemy))
    {
        AEnemyBarrier* Barrier =
            FindProtectingBarrier(Enemy);

        if (IsValid(Barrier))
        {
            const float Multiplier =
                GetBarrierDamageMultiplier(this);

            const float BarrierDamage =
                ProjectileDamage * Multiplier;

            const float HPBefore =
                Barrier->CurrentBarrierHP;

            Barrier->AbsorbDamage(BarrierDamage);

            UE_LOG(
                LogTemp,
                Log,
                TEXT("[Arrow] Barrier hit | Target: %s | Damage: %.1f | HP: %.1f -> %.1f"),
                *GetNameSafe(Enemy),
                BarrierDamage,
                HPBefore,
                Barrier->CurrentBarrierHP
            );

            // The barrier absorbed this projectile.
            // Do not damage the protected enemy.
            Destroy();
            return;
        }
    }

    // No active barrier: normal damage.
    Target->ReceiveDamage(ProjectileDamage);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Arrow] Hit %s | Damage: %.1f"),
        *GetNameSafe(Target),
        ProjectileDamage
    );

    Destroy();
}
