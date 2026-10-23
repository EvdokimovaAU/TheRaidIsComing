
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveManager.generated.h"

class ATargetPoint;


// ==========================================
// WAVE STATE
// ==========================================

UENUM(BlueprintType)
enum class EWaveState : uint8
{
    Waiting     UMETA(DisplayName = "Waiting"),
    Spawning    UMETA(DisplayName = "Spawning"),
    InProgress  UMETA(DisplayName = "In Progress"),
    Completed   UMETA(DisplayName = "Completed")
};


// ==========================================
// ONE SPAWN GROUP
// ==========================================

USTRUCT(BlueprintType)
struct FWaveSpawnGroup
{
    GENERATED_BODY()

    // Spawn point placed on the level
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Group")
    TObjectPtr<ATargetPoint> SpawnPoint = nullptr;

    // Enemies spawning from this specific point
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Group",
        meta = (ClampMin = "0"))
    int32 SwordsmanCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Group",
        meta = (ClampMin = "0"))
    int32 ArcherCount = 0;
};


// ==========================================
// ONE WAVE
// ==========================================

USTRUCT(BlueprintType)
struct FWaveData
{
    GENERATED_BODY()

    // Each group has its own point and enemy counts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
    TArray<FWaveSpawnGroup> SpawnGroups;

    // Shared interval between spawn attempts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave",
        meta = (ClampMin = "0.1"))
    float SpawnInterval = 1.5f;
};


// ==========================================
// EVENTS
// ==========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWaveStarted,
    int32,
    WaveNumber
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWaveCompleted,
    int32,
    WaveNumber
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FOnAllWavesCompleted
);


// ==========================================
// WAVE MANAGER
// ==========================================

UCLASS()
class RAIDISCOMING_API AWaveManager : public AActor
{
    GENERATED_BODY()

public:

    AWaveManager();

    // Configure on the placed actor, because
    // SpawnGroups reference TargetPoints from the level.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Waves")
    TArray<FWaveData> Waves;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    int32 CurrentWaveIndex = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    EWaveState WaveState = EWaveState::Waiting;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    int32 AliveEnemies = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    bool bSpawningFinished = false;


    // Events used by other game systems
    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnWaveStarted OnWaveStarted;

    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnWaveCompleted OnWaveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnAllWavesCompleted OnAllWavesCompleted;


    UFUNCTION(BlueprintCallable, Category = "Waves")
    void StartNextWave();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void RegisterEnemy();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void NotifyEnemyDied();

    UFUNCTION(BlueprintCallable, Category = "Waves")
    void FinishSpawning();

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetCurrentWaveNumber() const;

    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetTotalWaves() const;


protected:

    virtual void BeginPlay() override;


private:

    void CheckWaveCompletion();
};
