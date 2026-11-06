
#include "AI/MageProjectile.h"

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

AMageProjectile::AMageProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );

    RootComponent = SceneRoot;

    SetActorEnableCollision(false);

    InitialLifeSpan = 10.0f;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();
}

// ==========================================
// INITIALIZATION
// ==========================================

void AMageProjectile::InitializeProjectile(
    AAIBase* NewTarget,
    float NewDamage,
    float NewSplashRadius)
{
    if (!IsValid(NewTarget) || !NewTarget->IsAlive())
    {
        Destroy();
        return;
    }

    TargetActor = NewTarget;

    ProjectileDamage = FMath::Max(
        0.0f,
        NewDamage
    );

    ProjectileSplashRadius = FMath::Max(
        0.0f,
        NewSplashRadius
    );

    bHasExploded = false;
    bInitialized = true;
}

// ==========================================
// MOVEMENT
// ==========================================

void AMageProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bInitialized || bHasExploded)
    {
        return;
    }

    if (!TargetActor.IsValid() ||
        !TargetActor->IsAlive())
    {
        Destroy();
        return;
    }

    const FVector CurrentLocation =
        GetActorLocation();

    const FVector TargetLocation =
        TargetActor->GetActorLocation();

    const FVector ToTarget =
        TargetLocation - CurrentLocation;

    const float Distance = ToTarget.Size();

    if (Distance <= HitRadius)
    {
        HitTarget();
        return;
    }

    const FVector Direction =
        ToTarget.GetSafeNormal();

    const float Step =
        FMath::Max(FlightSpeed, 1.0f) * DeltaTime;

    if (Step >= Distance)
    {
        SetActorLocation(TargetLocation);
        HitTarget();
        return;
    }

    const FVector NewLocation =
        CurrentLocation + Direction * Step;

    SetActorLocation(NewLocation);

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }
}

// ==========================================
// HIT
// ==========================================

void AMageProjectile::HitTarget()
{
    if (!bInitialized || bHasExploded)
    {
        return;
    }

    bHasExploded = true;
    bInitialized = false;

    const FVector ImpactLocation =
        GetActorLocation();

    ApplySplashDamage(ImpactLocation);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[MageProjectile] Explosion | Damage: %.1f | Radius: %.1f"),
        ProjectileDamage,
        ProjectileSplashRadius
    );

    Destroy();
}

// ==========================================
// SPLASH DAMAGE
// ==========================================

void AMageProjectile::ApplySplashDamage(
    const FVector& ImpactLocation)
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    // Snapshot of enemies and their protection
    // before any damage is applied.
    TArray<TWeakObjectPtr<AEnemyAI>> EnemiesInRange;

    TSet<TWeakObjectPtr<AEnemyAI>> ProtectedEnemies;

    TSet<TWeakObjectPtr<AEnemyBarrier>> HitBarriers;

    for (TActorIterator<AEnemyAI> It(World); It; ++It)
    {
        AEnemyAI* Enemy = *It;

        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        const float Distance = FVector::Dist(
            ImpactLocation,
            Enemy->GetActorLocation()
        );

        if (Distance > ProjectileSplashRadius)
        {
            continue;
        }

        EnemiesInRange.Add(Enemy);

        AEnemyBarrier* Barrier =
            FindProtectingBarrier(Enemy);

        if (IsValid(Barrier))
        {
            ProtectedEnemies.Add(Enemy);
            HitBarriers.Add(Barrier);
        }
    }

    const float Multiplier =
        GetBarrierDamageMultiplier(this);

    const float BarrierDamage =
        ProjectileDamage * Multiplier;

    // Each affected barrier takes damage only once.
    for (const TWeakObjectPtr<AEnemyBarrier>& BarrierPtr
        : HitBarriers)
    {
        AEnemyBarrier* Barrier = BarrierPtr.Get();

        if (!IsValid(Barrier) ||
            !Barrier->IsBarrierActive())
        {
            continue;
        }

        const float HPBefore =
            Barrier->CurrentBarrierHP;

        Barrier->AbsorbDamage(BarrierDamage);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("[MageProjectile] Barrier hit | Damage: %.1f | HP: %.1f -> %.1f"),
            BarrierDamage,
            HPBefore,
            Barrier->CurrentBarrierHP
        );
    }

    int32 HitEnemiesCount = 0;
    int32 BlockedEnemiesCount = 0;

    for (const TWeakObjectPtr<AEnemyAI>& EnemyPtr
        : EnemiesInRange)
    {
        AEnemyAI* Enemy = EnemyPtr.Get();

        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        // An enemy protected at the beginning of
        // this explosion cannot take its damage.
        // This remains true if the barrier breaks.
        if (ProtectedEnemies.Contains(EnemyPtr))
        {
            ++BlockedEnemiesCount;
            continue;
        }

        Enemy->ReceiveDamage(ProjectileDamage);

        ++HitEnemiesCount;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[MageProjectile] Splash hit: %s | Damage: %.1f"),
            *Enemy->GetName(),
            ProjectileDamage
        );
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[MageProjectile] Explosion result | Enemies hit: %d | Protected: %d | Barriers hit: %d"),
        HitEnemiesCount,
        BlockedEnemiesCount,
        HitBarriers.Num()
    );
}
