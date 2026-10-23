
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "EnemySpawner.generated.h"

class AWaveManager;

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


    // WaveManager placed on the level
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawner")
    TObjectPtr<AWaveManager> WaveManager = nullptr;


    // Capitol placed on the level
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Spawner")
    TObjectPtr<AActor> Capitol = nullptr;


    // Enemy Blueprint classes
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Enemies")
    TSubclassOf<AActor> SwordsmanClass;


    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Enemies")
    TSubclassOf<AActor> ArcherClass;


private:

    // Called when WaveManager starts a wave
    UFUNCTION()
    void HandleWaveStarted(int32 WaveNumber);

    // Spawns one enemy
    void SpawnNextEnemy();


    FTimerHandle SpawnTimer;

    int32 RemainingSwordsmen = 0;

    int32 RemainingArchers = 0;

    float CurrentSpawnInterval = 1.5f;

    bool bIsSpawning = false;
};
