
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameBalanceManager.generated.h"

// ============================================
// COMMON ENEMY BALANCE
// ============================================

USTRUCT(BlueprintType)
struct FEnemyBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Stats", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.0"))
    float MovementSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.0"))
    float AttackDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.01"))
    float AttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Economy", meta = (ClampMin = "0"))
    int32 KillReward = 50;
};

// ============================================
// SWORDSMAN
// ============================================

USTRUCT(BlueprintType)
struct FSwordsmanBalanceSettings : public FEnemyBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Walls", meta = (ClampMin = "0.0"))
    float WallDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Walls", meta = (ClampMin = "0.01"))
    float WallAttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Walls", meta = (ClampMin = "0.0"))
    float WallAttackDistance = 115.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Walls", meta = (ClampMin = "0.0"))
    float WallDetectionDistance = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.0"))
    float WallApproachSpeed = 250.0f;
};

// ============================================
// MAGE
// ============================================

USTRUCT(BlueprintType)
struct FMageBalanceSettings : public FEnemyBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Barrier", meta = (ClampMin = "1.0"))
    float BarrierHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Barrier", meta = (ClampMin = "0.0"))
    float BarrierCooldown = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Barrier", meta = (ClampMin = "0.0"))
    float BarrierDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Barrier", meta = (ClampMin = "0.0"))
    float BarrierSpawnDelay = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Support", meta = (ClampMin = "0.0"))
    float AllyDetectionRadius = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Support", meta = (ClampMin = "0.0"))
    float FollowDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.0"))
    float MageAcceleration = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.0"))
    float MageBrakingDeceleration = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.0"))
    float SpeedChangeRate = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Movement", meta = (ClampMin = "0.01"))
    float FollowUpdateInterval = 0.2f;
};

// ============================================
// TORCHBEARER
// ============================================

USTRUCT(BlueprintType)
struct FTorchbearerBalanceSettings : public FEnemyBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float FireRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float IgniteDelay = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float ReigniteDelay = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float WallDetectionDistance = 180.0f;
};

// ============================================
// TOWER
// ============================================

USTRUCT(BlueprintType)
struct FTowerBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Stats", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.0"))
    float Damage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.0"))
    float AttackRange = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.01"))
    float AttackCooldown = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat", meta = (ClampMin = "0.01"))
    float TargetCheckInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile", meta = (ClampMin = "0.0"))
    float ProjectileSpeed = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile", meta = (ClampMin = "0.0"))
    float SplashRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Economy", meta = (ClampMin = "0"))
    int32 BuildCost = 50;
};

// ============================================
// SPAWNING
// ============================================

USTRUCT(BlueprintType)
struct FSpawnBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Placement", meta = (ClampMin = "0.0"))
    float MinimumSpawnDistance = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Placement", meta = (ClampMin = "0.0"))
    float SpawnSearchRadius = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Placement", meta = (ClampMin = "1"))
    int32 MaxSpawnAttempts = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Placement", meta = (ClampMin = "0.01"))
    float RetryInterval = 0.5f;
};

// ============================================
// BURNING
// ============================================

USTRUCT(BlueprintType)
struct FBurningBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float BurnDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Fire", meta = (ClampMin = "0.0"))
    float DamagePerSecond = 2.0f;
};

// ============================================
// GENERAL
// ============================================

USTRUCT(BlueprintType)
struct FGeneralBalanceSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Economy", meta = (ClampMin = "0"))
    int32 StartingGold = 300;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Economy", meta = (ClampMin = "0"))
    int32 WaveCompletionReward = 100;

    // 0.0 = barrier takes no damage from towers.
    // 0.5 = barrier takes half of normal damage.
    // 1.0 = barrier takes full normal damage.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Combat|Barrier",
        meta = (ClampMin = "0.0", ClampMax = "1.0",
            UIMin = "0.0", UIMax = "1.0"))
    float BarrierDamageMultiplier = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Buildings", meta = (ClampMin = "1.0"))
    float CapitolMaxHealth = 100.0f;
};

// ============================================
// GAME BALANCE MANAGER
// ============================================

UCLASS(BlueprintType, Blueprintable)
class RAIDISCOMING_API AGameBalanceManager : public AActor
{
    GENERATED_BODY()

public:
    AGameBalanceManager();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "01 Enemies|Swordsman")
    FSwordsmanBalanceSettings Swordsman;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "01 Enemies|Mage")
    FMageBalanceSettings Mage;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "01 Enemies|Torchbearer")
    FTorchbearerBalanceSettings Torchbearer;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "02 Towers|Archer")
    FTowerBalanceSettings ArcherTower;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "02 Towers|Mage")
    FTowerBalanceSettings MageTower;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "03 Spawning")
    FSpawnBalanceSettings Spawning;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "04 Burning")
    FBurningBalanceSettings Burning;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "05 General")
    FGeneralBalanceSettings General;

    UFUNCTION(BlueprintPure, Category = "Game Balance",
        meta = (WorldContext = "WorldContextObject"))
    static AGameBalanceManager* GetBalanceManager(
        const UObject* WorldContextObject
    );

protected:
    virtual void BeginPlay() override;
};
