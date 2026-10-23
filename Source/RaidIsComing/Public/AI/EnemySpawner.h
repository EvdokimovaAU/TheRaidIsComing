
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "EnemySpawner.generated.h"

class AWaveManager;
class AEnemyAI;
class ATargetPoint;


// Runtime state for one spawn group
struct FActiveSpawnGroup
{
    TWeakObjectPtr<ATargetPoint> SpawnPoint;

    int32 RemainingSwordsmen = 0;
    int32 RemainingArchers = 0;

    bool HasPendingEnemies() const
    {
        return RemainingSwordsmen > 0 ||
            RemainingArchers > 0;
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


    // ======================================
    // LEVEL REFERENCES
    // ======================================

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawner")
    TObjectPtr<AWaveManager> WaveManager = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawner")
    TObjectPtr<AActor> Capitol = nullptr;


    // ======================================
    // ENEMY CLASSES
    // ======================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Enemies")
    TSubclassOf<AEnemyAI> SwordsmanClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Enemies")
    TSubclassOf<AEnemyAI> ArcherClass;


    // ======================================
    // SPAWN PLACEMENT
    // ======================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Placement",
        meta = (ClampMin = "0.0"))
    float MinimumSpawnDistance = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Placement",
        meta = (ClampMin = "0.0"))
    float SpawnSearchRadius = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Placement",
        meta = (ClampMin = "1"))
    int32 MaxSpawnAttempts = 24;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Placement",
        meta = (ClampMin = "0.1"))
    float RetryInterval = 0.5f;


private:

    // ======================================
    // WAVE EVENTS
    // ======================================

    UFUNCTION()
    void HandleWaveStarted(int32 WaveNumber);

    UFUNCTION()
    void HandleEnemyDied(AEnemyAI* DeadEnemy);


    // ======================================
    // SPAWNING
    // ======================================

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


    // ======================================
    // RUNTIME DATA
    // ======================================

    TArray<FActiveSpawnGroup> ActiveGroups;

    // Only enemies registered by this spawner.
    TSet<TWeakObjectPtr<AEnemyAI>> RegisteredEnemies;

    FTimerHandle SpawnTimer;

    int32 NextGroupIndex = 0;

    float CurrentSpawnInterval = 1.5f;

    bool bIsSpawning = false;
};
