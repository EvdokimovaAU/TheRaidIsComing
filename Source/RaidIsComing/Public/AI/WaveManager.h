
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "WaveManager.generated.h"

class ATargetPoint;

UENUM(BlueprintType)
enum class EWaveState : uint8
{
    Waiting    UMETA(DisplayName = "Waiting"),
    Spawning   UMETA(DisplayName = "Spawning"),
    InProgress UMETA(DisplayName = "In Progress"),
    Completed  UMETA(DisplayName = "Completed")
};

USTRUCT(BlueprintType)
struct FWaveSpawnGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn Group")
    TObjectPtr<ATargetPoint> SpawnPoint = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn Group",
        meta = (ClampMin = "0"))
    int32 SwordsmanCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Spawn Group",
        meta = (ClampMin = "0"))
    int32 MageCount = 0;
};

USTRUCT(BlueprintType)
struct FWaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Wave")
    TArray<FWaveSpawnGroup> SpawnGroups;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Wave",
        meta = (ClampMin = "0.1"))
    float SpawnInterval = 1.5f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWaveStarted, int32, WaveNumber);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWaveCompleted, int32, WaveNumber);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FOnAllWavesCompleted);

UCLASS()
class RAIDISCOMING_API AWaveManager : public AActor
{
    GENERATED_BODY()

public:
    AWaveManager();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Waves")
    TArray<FWaveData> Waves;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Waves|Timing",
        meta = (ClampMin = "0.1"))
    float TimeBetweenWaves = 5.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Waves")
    int32 CurrentWaveIndex = -1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Waves")
    EWaveState WaveState = EWaveState::Waiting;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Waves")
    int32 AliveEnemies = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Waves")
    bool bSpawningFinished = false;

    UPROPERTY(BlueprintAssignable,
        Category = "Waves|Events")
    FOnWaveStarted OnWaveStarted;

    UPROPERTY(BlueprintAssignable,
        Category = "Waves|Events")
    FOnWaveCompleted OnWaveCompleted;

    UPROPERTY(BlueprintAssignable,
        Category = "Waves|Events")
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

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    void CheckWaveCompletion();

    FTimerHandle NextWaveTimerHandle;
};
