
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "EnemySpawner.generated.h"

class AWaveManager;
class AEnemyAI;
class ATargetPoint;

struct FActiveSpawnGroup
{
    TWeakObjectPtr<ATargetPoint> SpawnPoint;

    int32 RemainingSwordsmen = 0;
    int32 RemainingMages = 0;

    bool HasPendingEnemies() const
    {
        return RemainingSwordsmen > 0 ||
            RemainingMages > 0;
    }
};

UCLASS()
class RAIDISCOMING_API AEnemySpawner : public AActor
{
    GENERATED_BODY()

public:
    AEnemySpawner();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Spawner")
    TObjectPtr<AWaveManager> WaveManager = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Spawner")
    TObjectPtr<AActor> Capitol = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Enemies")
    TSubclassOf<AEnemyAI> SwordsmanClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Enemies")
    TSubclassOf<AEnemyAI> MageClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Placement",
        meta = (ClampMin = "0.0"))
    float MinimumSpawnDistance = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Placement",
        meta = (ClampMin = "0.0"))
    float SpawnSearchRadius = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Placement",
        meta = (ClampMin = "1"))
    int32 MaxSpawnAttempts = 24;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Spawner|Placement",
        meta = (ClampMin = "0.1"))
    float RetryInterval = 0.5f;

private:
    UFUNCTION()
    void HandleWaveStarted(int32 WaveNumber);

    UFUNCTION()
    void HandleEnemyDied(AEnemyAI* DeadEnemy);

    void SpawnNextEnemy();

    bool TrySpawnFromGroup(FActiveSpawnGroup& Group);

    bool FindFreeSpawnLocation(
        ATargetPoint* Point,
        TSubclassOf<AEnemyAI> EnemyClass,
        FVector& OutLocation
    ) const;

    bool IsFarEnoughFromEnemies(
        const FVector& Location
    ) const;

    bool HasPendingEnemies() const;

    void CompleteSpawning();

    TArray<FActiveSpawnGroup> ActiveGroups;

    TSet<TWeakObjectPtr<AEnemyAI>> RegisteredEnemies;

    FTimerHandle SpawnTimer;

    int32 NextGroupIndex = 0;

    float CurrentSpawnInterval = 1.5f;

    bool bIsSpawning = false;
};
