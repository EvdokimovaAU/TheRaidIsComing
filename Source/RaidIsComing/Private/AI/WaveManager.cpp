
#include "AI/WaveManager.h"

AWaveManager::AWaveManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AWaveManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Initialized. Total waves: %d"),
        Waves.Num()
    );
}

void AWaveManager::StartNextWave()
{
    // Do not start another wave while current one is active
    if (WaveState == EWaveState::Spawning ||
        WaveState == EWaveState::InProgress)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] Cannot start next wave: current wave is active"));

        return;
    }

    const int32 NextIndex = CurrentWaveIndex + 1;

    if (!Waves.IsValidIndex(NextIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] All waves completed"));

        return;
    }

    CurrentWaveIndex = NextIndex;

    AliveEnemies = 0;
    bSpawningFinished = false;

    WaveState = EWaveState::Spawning;

    const FWaveData& CurrentWave = Waves[CurrentWaveIndex];

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d started. Swordsmen: %d | Archers: %d"),
        GetCurrentWaveNumber(),
        CurrentWave.SwordsmanCount,
        CurrentWave.ArcherCount
    );

    OnWaveStarted.Broadcast(GetCurrentWaveNumber());

    // EnemySpawner will be connected here later
}

void AWaveManager::RegisterEnemy()
{
    if (WaveState != EWaveState::Spawning)
    {
        return;
    }

    ++AliveEnemies;

    UE_LOG(LogTemp, Log,
        TEXT("[WaveManager] Enemy registered. Alive: %d"),
        AliveEnemies
    );
}

void AWaveManager::NotifyEnemyDied()
{
    if (WaveState != EWaveState::Spawning &&
        WaveState != EWaveState::InProgress)
    {
        return;
    }

    AliveEnemies = FMath::Max(0, AliveEnemies - 1);

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Enemy died. Remaining: %d"),
        AliveEnemies
    );

    CheckWaveCompletion();
}

void AWaveManager::FinishSpawning()
{
    if (WaveState != EWaveState::Spawning)
    {
        return;
    }

    bSpawningFinished = true;

    WaveState = EWaveState::InProgress;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Spawning finished. Alive enemies: %d"),
        AliveEnemies
    );

    CheckWaveCompletion();
}

void AWaveManager::CheckWaveCompletion()
{
    if (!bSpawningFinished || AliveEnemies > 0)
    {
        return;
    }

    if (WaveState != EWaveState::InProgress)
    {
        return;
    }

    WaveState = EWaveState::Completed;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d completed!"),
        GetCurrentWaveNumber()
    );

    OnWaveCompleted.Broadcast(GetCurrentWaveNumber());

    // Check if this was the last wave
    if (CurrentWaveIndex == Waves.Num() - 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] ALL WAVES COMPLETED!"));

        OnAllWavesCompleted.Broadcast();
    }
}

int32 AWaveManager::GetCurrentWaveNumber() const
{
    return CurrentWaveIndex + 1;
}

int32 AWaveManager::GetTotalWaves() const
{
    return Waves.Num();
}
