
#include "AI/EnemySpawner.h"

#include "AI/WaveManager.h"
#include "AI/EnemyAI.h"

#include "Engine/World.h"


// ==========================================
// CONSTRUCTOR
// ==========================================

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}


// ==========================================
// BEGIN PLAY
// ==========================================

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(WaveManager))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] WaveManager is not assigned!"));

        return;
    }

    if (!IsValid(Capitol))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Capitol is not assigned!"));

        return;
    }

    // Subscribe to WaveManager event
    WaveManager->OnWaveStarted.AddDynamic(
        this,
        &AEnemySpawner::HandleWaveStarted
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Successfully connected to WaveManager"));

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Capitol assigned: %s"),
        *Capitol->GetName()
    );
}


// ==========================================
// WAVE STARTED
// ==========================================

void AEnemySpawner::HandleWaveStarted(int32 WaveNumber)
{
    if (!IsValid(WaveManager) || bIsSpawning)
    {
        return;
    }

    const int32 WaveIndex = WaveNumber - 1;

    if (!WaveManager->Waves.IsValidIndex(WaveIndex))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Invalid wave index: %d"),
            WaveIndex);

        return;
    }

    const FWaveData& WaveData =
        WaveManager->Waves[WaveIndex];

    RemainingSwordsmen =
        FMath::Max(0, WaveData.SwordsmanCount);

    RemainingArchers =
        FMath::Max(0, WaveData.ArcherCount);

    CurrentSpawnInterval =
        FMath::Max(0.1f, WaveData.SpawnInterval);

    bIsSpawning = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Wave %d: %d swordsmen, %d archers"),
        WaveNumber,
        RemainingSwordsmen,
        RemainingArchers
    );

    // Spawn first enemy immediately
    SpawnNextEnemy();

    // Start timer for remaining enemies
    if (bIsSpawning)
    {
        GetWorldTimerManager().SetTimer(
            SpawnTimer,
            this,
            &AEnemySpawner::SpawnNextEnemy,
            CurrentSpawnInterval,
            true
        );
    }
}


// ==========================================
// SPAWN NEXT ENEMY
// ==========================================

void AEnemySpawner::SpawnNextEnemy()
{
    if (!bIsSpawning || !IsValid(WaveManager))
    {
        return;
    }

    TSubclassOf<AActor> ClassToSpawn = nullptr;


    // First spawn swordsmen
    if (RemainingSwordsmen > 0)
    {
        ClassToSpawn = SwordsmanClass;

        --RemainingSwordsmen;
    }

    // Then spawn archers
    else if (RemainingArchers > 0)
    {
        ClassToSpawn = ArcherClass;

        --RemainingArchers;
    }

    // All enemies have been spawned
    else
    {
        GetWorldTimerManager().ClearTimer(SpawnTimer);

        bIsSpawning = false;

        UE_LOG(LogTemp, Warning,
            TEXT("[EnemySpawner] All enemies spawned"));

        WaveManager->FinishSpawning();

        return;
    }


    // Check enemy class
    if (!ClassToSpawn)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Enemy class is not assigned!"));

        return;
    }


    // Spawn settings
    FActorSpawnParameters SpawnParams;

    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;


    // Create enemy at EnemySpawner location
    AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
        ClassToSpawn,
        GetActorTransform(),
        SpawnParams
    );


    // ======================================
    // INITIALIZE SPAWNED ENEMY
    // ======================================

    if (IsValid(SpawnedEnemy))
    {
        // Check if spawned actor is our EnemyAI
        if (AEnemyAI* EnemyAI = Cast<AEnemyAI>(SpawnedEnemy))
        {
            // Assign Capitol
            if (IsValid(Capitol))
            {
                EnemyAI->SetCapitol(Capitol.Get());

                UE_LOG(LogTemp, Warning,
                    TEXT("[EnemySpawner] Capitol assigned to %s"),
                    *EnemyAI->GetName()
                );
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("[EnemySpawner] Capitol is invalid!"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error,
                TEXT("[EnemySpawner] Spawned actor is not EnemyAI!"));
        }


        // Register enemy in WaveManager
        WaveManager->RegisterEnemy();


        UE_LOG(LogTemp, Warning,
            TEXT("[EnemySpawner] Spawned: %s"),
            *SpawnedEnemy->GetName()
        );
    }
    else
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Failed to spawn enemy"));
    }
}


// ==========================================
// END PLAY
// ==========================================

void AEnemySpawner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SpawnTimer);

    if (IsValid(WaveManager))
    {
        WaveManager->OnWaveStarted.RemoveDynamic(
            this,
            &AEnemySpawner::HandleWaveStarted
        );
    }

    Super::EndPlay(EndPlayReason);
}
