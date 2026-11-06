
#include "AI/GameBalanceManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AGameBalanceManager::AGameBalanceManager()
{
    PrimaryActorTick.bCanEverTick = false;

    SetActorEnableCollision(false);

    // ========================================
    // SWORDSMAN DEFAULTS
    // ========================================

    Swordsman.MaxHealth = 100.0f;
    Swordsman.MovementSpeed = 300.0f;
    Swordsman.AttackDamage = 10.0f;
    Swordsman.AttackRange = 150.0f;
    Swordsman.AttackCooldown = 1.0f;
    Swordsman.KillReward = 50;

    Swordsman.WallDamage = 10.0f;
    Swordsman.WallAttackCooldown = 1.0f;
    Swordsman.WallAttackDistance = 115.0f;
    Swordsman.WallDetectionDistance = 220.0f;
    Swordsman.WallApproachSpeed = 250.0f;

    // ========================================
    // MAGE DEFAULTS
    // ========================================

    Mage.MaxHealth = 100.0f;
    Mage.MovementSpeed = 250.0f;
    Mage.AttackDamage = 10.0f;
    Mage.AttackRange = 150.0f;
    Mage.AttackCooldown = 1.0f;
    Mage.KillReward = 50;

    Mage.BarrierHealth = 100.0f;
    Mage.BarrierCooldown = 5.0f;
    Mage.BarrierDuration = 0.0f;
    Mage.BarrierSpawnDelay = 0.45f;

    Mage.AllyDetectionRadius = 800.0f;
    Mage.FollowDistance = 180.0f;
    Mage.MageAcceleration = 350.0f;
    Mage.MageBrakingDeceleration = 300.0f;
    Mage.SpeedChangeRate = 150.0f;
    Mage.FollowUpdateInterval = 0.2f;

    // ========================================
    // TORCHBEARER DEFAULTS
    // ========================================

    Torchbearer.MaxHealth = 100.0f;
    Torchbearer.MovementSpeed = 300.0f;
    Torchbearer.AttackDamage = 10.0f;
    Torchbearer.AttackRange = 150.0f;
    Torchbearer.AttackCooldown = 1.0f;
    Torchbearer.KillReward = 50;

    Torchbearer.FireRange = 150.0f;
    Torchbearer.IgniteDelay = 0.6f;
    Torchbearer.ReigniteDelay = 1.0f;
    Torchbearer.WallDetectionDistance = 180.0f;

    // ========================================
    // ARCHER TOWER DEFAULTS
    // ========================================

    ArcherTower.MaxHealth = 100.0f;
    ArcherTower.Damage = 10.0f;
    ArcherTower.AttackRange = 500.0f;
    ArcherTower.AttackCooldown = 2.5f;
    ArcherTower.TargetCheckInterval = 0.2f;
    ArcherTower.ProjectileSpeed = 1000.0f;
    ArcherTower.SplashRadius = 0.0f;
    ArcherTower.BuildCost = 50;

    // ========================================
    // MAGE TOWER DEFAULTS
    // ========================================

    MageTower.MaxHealth = 100.0f;
    MageTower.Damage = 20.0f;
    MageTower.AttackRange = 450.0f;
    MageTower.AttackCooldown = 3.0f;
    MageTower.TargetCheckInterval = 0.2f;
    MageTower.ProjectileSpeed = 1000.0f;
    MageTower.SplashRadius = 200.0f;
    MageTower.BuildCost = 50;

    // ========================================
    // SPAWNING DEFAULTS
    // ========================================

    Spawning.MinimumSpawnDistance = 150.0f;
    Spawning.SpawnSearchRadius = 400.0f;
    Spawning.MaxSpawnAttempts = 24;
    Spawning.RetryInterval = 0.5f;

    // ========================================
    // BURNING DEFAULTS
    // ========================================

    Burning.BurnDuration = 5.0f;
    Burning.DamagePerSecond = 2.0f;

    // ========================================
    // GENERAL DEFAULTS
    // ========================================

    General.StartingGold = 300;
    General.CapitolMaxHealth = 100.0f;
}

void AGameBalanceManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Manager initialized: %s"),
        *GetName());

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Swordsman | HP=%.1f Speed=%.1f Damage=%.1f"),
        Swordsman.MaxHealth,
        Swordsman.MovementSpeed,
        Swordsman.AttackDamage);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Mage | HP=%.1f Speed=%.1f BarrierHP=%.1f"),
        Mage.MaxHealth,
        Mage.MovementSpeed,
        Mage.BarrierHealth);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Torchbearer | HP=%.1f Speed=%.1f FireRange=%.1f"),
        Torchbearer.MaxHealth,
        Torchbearer.MovementSpeed,
        Torchbearer.FireRange);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] ArcherTower | Damage=%.1f Range=%.1f Cooldown=%.2f"),
        ArcherTower.Damage,
        ArcherTower.AttackRange,
        ArcherTower.AttackCooldown);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] MageTower | Damage=%.1f Range=%.1f Splash=%.1f"),
        MageTower.Damage,
        MageTower.AttackRange,
        MageTower.SplashRadius);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Spawning | Distance=%.1f Radius=%.1f Attempts=%d"),
        Spawning.MinimumSpawnDistance,
        Spawning.SpawnSearchRadius,
        Spawning.MaxSpawnAttempts);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] Burning | Duration=%.1f DPS=%.1f"),
        Burning.BurnDuration,
        Burning.DamagePerSecond);

    UE_LOG(LogTemp, Log,
        TEXT("[GameBalance] General | Gold=%d CapitolHP=%.1f"),
        General.StartingGold,
        General.CapitolMaxHealth);
}

AGameBalanceManager* AGameBalanceManager::GetBalanceManager(
    const UObject* WorldContextObject)
{
    if (!IsValid(WorldContextObject))
    {
        return nullptr;
    }

    UWorld* World = WorldContextObject->GetWorld();

    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AGameBalanceManager> It(World); It; ++It)
    {
        return *It;
    }

    return nullptr;
}
