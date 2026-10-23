
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
        Waves.Num());
}


// ==========================================
// START NEXT WAVE
// ==========================================

void AWaveManager::StartNextWave()
{
    if (WaveState == EWaveState::Spawning ||
        WaveState == EWaveState::InProgress)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] Current wave is still active"));

        return;
    }

    const int32 NextIndex = CurrentWaveIndex + 1;

    if (!Waves.IsValidIndex(NextIndex))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] No more waves"));

        return;
    }

    const FWaveData& NextWave = Waves[NextIndex];

    int32 TotalSwordsmen = 0;
    int32 TotalArchers = 0;

    for (const FWaveSpawnGroup& Group : NextWave.SpawnGroups)
    {
        TotalSwordsmen += FMath::Max(0, Group.SwordsmanCount);
        TotalArchers += FMath::Max(0, Group.ArcherCount);
    }

    CurrentWaveIndex = NextIndex;

    AliveEnemies = 0;
    bSpawningFinished = false;

    WaveState = EWaveState::Spawning;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d started. Swordsmen: %d | Archers: %d | Groups: %d"),
        GetCurrentWaveNumber(),
        TotalSwordsmen,
        TotalArchers,
        NextWave.SpawnGroups.Num());

    OnWaveStarted.Broadcast(GetCurrentWaveNumber());
}


// ==========================================
// REGISTER ENEMY
// ==========================================

void AWaveManager::RegisterEnemy()
{
    if (WaveState != EWaveState::Spawning)
    {
        return;
    }

    ++AliveEnemies;

    UE_LOG(LogTemp, Log,
        TEXT("[WaveManager] Enemy registered. Alive: %d"),
        AliveEnemies);
}


// ==========================================
// ENEMY DIED
// ==========================================

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
        AliveEnemies);

    CheckWaveCompletion();
}


// ==========================================
// FINISH SPAWNING
// ==========================================

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
        AliveEnemies);

    CheckWaveCompletion();
}


// ==========================================
// CHECK COMPLETION
// ==========================================

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
        GetCurrentWaveNumber());

    OnWaveCompleted.Broadcast(GetCurrentWaveNumber());

    if (CurrentWaveIndex == Waves.Num() - 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] ALL WAVES COMPLETED!"));

        OnAllWavesCompleted.Broadcast();
    }
}


// ==========================================
// GETTERS
// ==========================================

int32 AWaveManager::GetCurrentWaveNumber() const
{
    return CurrentWaveIndex + 1;
}


int32 AWaveManager::GetTotalWaves() const
{
    return Waves.Num();
}
