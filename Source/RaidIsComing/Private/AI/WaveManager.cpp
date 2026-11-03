
#include "AI/WaveManager.h"

#include "Engine/World.h"
#include "TimerManager.h"

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

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Time between waves: %.1f seconds"),
        TimeBetweenWaves);
}

void AWaveManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        NextWaveTimerHandle);

    Super::EndPlay(EndPlayReason);
}

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

    GetWorldTimerManager().ClearTimer(
        NextWaveTimerHandle);

    const FWaveData& NextWave = Waves[NextIndex];

    int32 TotalSwordsmen = 0;
    int32 TotalMages = 0;

    for (const FWaveSpawnGroup& Group :
        NextWave.SpawnGroups)
    {
        TotalSwordsmen += FMath::Max(
            0, Group.SwordsmanCount);

        TotalMages += FMath::Max(
            0, Group.MageCount);
    }

    CurrentWaveIndex = NextIndex;

    AliveEnemies = 0;
    bSpawningFinished = false;
    WaveState = EWaveState::Spawning;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d started. Swordsmen: %d | Mages: %d | Groups: %d"),
        GetCurrentWaveNumber(),
        TotalSwordsmen,
        TotalMages,
        NextWave.SpawnGroups.Num());

    OnWaveStarted.Broadcast(GetCurrentWaveNumber());
}

void AWaveManager::RegisterEnemy()
{
    if (WaveState != EWaveState::Spawning)
        return;

    ++AliveEnemies;

    UE_LOG(LogTemp, Log,
        TEXT("[WaveManager] Enemy registered. Alive: %d"),
        AliveEnemies);
}

void AWaveManager::NotifyEnemyDied()
{
    if (WaveState != EWaveState::Spawning &&
        WaveState != EWaveState::InProgress)
    {
        return;
    }

    AliveEnemies = FMath::Max(
        0, AliveEnemies - 1);

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Enemy died. Remaining: %d"),
        AliveEnemies);

    CheckWaveCompletion();
}

void AWaveManager::FinishSpawning()
{
    if (WaveState != EWaveState::Spawning)
        return;

    bSpawningFinished = true;
    WaveState = EWaveState::InProgress;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Spawning finished. Alive enemies: %d"),
        AliveEnemies);

    CheckWaveCompletion();
}

void AWaveManager::CheckWaveCompletion()
{
    if (!bSpawningFinished ||
        AliveEnemies > 0 ||
        WaveState != EWaveState::InProgress)
    {
        return;
    }

    WaveState = EWaveState::Completed;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d completed!"),
        GetCurrentWaveNumber());

    OnWaveCompleted.Broadcast(GetCurrentWaveNumber());

    if (WaveState != EWaveState::Completed)
        return;

    if (CurrentWaveIndex == Waves.Num() - 1)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] ALL WAVES COMPLETED!"));

        OnAllWavesCompleted.Broadcast();
        return;
    }

    const float Delay = FMath::Max(
        0.1f, TimeBetweenWaves);

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Next wave starts in %.1f seconds"),
        Delay);

    GetWorldTimerManager().SetTimer(
        NextWaveTimerHandle,
        this,
        &AWaveManager::StartNextWave,
        Delay,
        false);
}

int32 AWaveManager::GetCurrentWaveNumber() const
{
    return CurrentWaveIndex + 1;
}

int32 AWaveManager::GetTotalWaves() const
{
    return Waves.Num();
}
