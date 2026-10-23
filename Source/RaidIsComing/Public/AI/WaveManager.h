
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveManager.generated.h"

// Current wave state
UENUM(BlueprintType)
enum class EWaveState : uint8
{
    Waiting     UMETA(DisplayName = "Waiting"),
    Spawning    UMETA(DisplayName = "Spawning"),
    InProgress  UMETA(DisplayName = "In Progress"),
    Completed   UMETA(DisplayName = "Completed")
};

// Settings for one wave
USTRUCT(BlueprintType)
struct FWaveData
{
    GENERATED_BODY()

    // Number of swordsmen
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
    int32 SwordsmanCount = 5;

    // Number of archers
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
    int32 ArcherCount = 2;

    // Delay between enemy spawns
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave",
        meta = (ClampMin = "0.1"))
    float SpawnInterval = 1.5f;
};

// Events for other game systems
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

UCLASS()
class RAIDISCOMING_API AWaveManager : public AActor
{
    GENERATED_BODY()

public:

    AWaveManager();

    // Wave configuration for this level
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
    TArray<FWaveData> Waves;

    // Current wave index (starts at -1)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    int32 CurrentWaveIndex = -1;

    // Current state
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    EWaveState WaveState = EWaveState::Waiting;

    // Number of enemies still alive
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    int32 AliveEnemies = 0;

    // Have all enemies of this wave been spawned?
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Waves")
    bool bSpawningFinished = false;

    // Events
    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnWaveStarted OnWaveStarted;

    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnWaveCompleted OnWaveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Waves|Events")
    FOnAllWavesCompleted OnAllWavesCompleted;

    // Start next wave
    UFUNCTION(BlueprintCallable, Category = "Waves")
    void StartNextWave();

    // Register spawned enemy
    UFUNCTION(BlueprintCallable, Category = "Waves")
    void RegisterEnemy();

    // Called when enemy dies
    UFUNCTION(BlueprintCallable, Category = "Waves")
    void NotifyEnemyDied();

    // Called when spawner finishes creating enemies
    UFUNCTION(BlueprintCallable, Category = "Waves")
    void FinishSpawning();

    // Current wave number for UI
    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetCurrentWaveNumber() const;

    // Total wave count
    UFUNCTION(BlueprintPure, Category = "Waves")
    int32 GetTotalWaves() const;

protected:

    virtual void BeginPlay() override;

private:

    void CheckWaveCompletion();
};
