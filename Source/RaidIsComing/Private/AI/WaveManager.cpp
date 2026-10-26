
#include "AI/WaveManager.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ==========================================
// CONSTRUCTOR
// ==========================================

AWaveManager::AWaveManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

// ==========================================
// BEGIN PLAY
// ==========================================

void AWaveManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Initialized. Total waves: %d"),
        Waves.Num());

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Time between waves: %.1f seconds"),
        TimeBetweenWaves);
}

// ==========================================
// END PLAY
// ==========================================

void AWaveManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(NextWaveTimerHandle);

    Super::EndPlay(EndPlayReason);
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

    // Prevent a pending timer from starting
    // an additional wave unexpectedly.
    GetWorldTimerManager().ClearTimer(NextWaveTimerHandle);

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
// CHECK WAVE COMPLETION
// ==========================================

void AWaveManager::CheckWaveCompletion()
{
    // Spawning must be finished.
    if (!bSpawningFinished)
    {
        return;
    }

    // All enemies must be dead.
    if (AliveEnemies > 0)
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

    // Notify day/night and other systems.
    OnWaveCompleted.Broadcast(GetCurrentWaveNumber());

    // ======================================
    // LAST WAVE
    // ======================================

    if (CurrentWaveIndex == Waves.Num() - 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] ALL WAVES COMPLETED!"));

        OnAllWavesCompleted.Broadcast();

        return;
    }

    // ======================================
    // SCHEDULE NEXT WAVE
    // ======================================

    // A listener of OnWaveCompleted may have
    // already started the next wave.
    if (WaveState != EWaveState::Completed)
    {
        return;
    }

    const float Delay = FMath::Max(
        0.1f,
        TimeBetweenWaves
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Next wave starts in %.1f seconds"),
        Delay);

    GetWorldTimerManager().SetTimer(
        NextWaveTimerHandle,
        this,
        &AWaveManager::StartNextWave,
        Delay,
        false
    );
}

// ==========================================
// GET CURRENT WAVE NUMBER
// ==========================================

int32 AWaveManager::GetCurrentWaveNumber() const
{
    return CurrentWaveIndex + 1;
}

// ==========================================
// GET TOTAL WAVES
// ==========================================

int32 AWaveManager::GetTotalWaves() const
{
    return Waves.Num();
}
