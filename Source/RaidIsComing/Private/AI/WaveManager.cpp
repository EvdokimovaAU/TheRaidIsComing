
#include "AI/WaveManager.h"

#include "AI/GameBalanceManager.h"
#include "Construction/RICMoneyComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// CONSTRUCTOR

AWaveManager::AWaveManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

// BEGIN PLAY

void AWaveManager::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Initialized. Total waves: %d"),
        Waves.Num());

    UE_LOG(LogTemp, Log,
        TEXT("[WaveManager] Preparation. Press Q to start wave 1."));
}

// END PLAY

void AWaveManager::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        NextWaveTimerHandle);

    Super::EndPlay(EndPlayReason);
}

// MANUAL START

void AWaveManager::StartNextWave()
{
    if (bDispatchingWaveEvents ||
        LevelPhase != ERICLevelPhase::Preparation)
    {
        return;
    }

    StartNextWaveInternal();
}

// START NEXT WAVE

void AWaveManager::StartNextWaveInternal()
{
    if (bDispatchingWaveEvents ||
        LevelPhase == ERICLevelPhase::Completed)
    {
        return;
    }

    TGuardValue<bool> EventGuard(
        bDispatchingWaveEvents, true);

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
    int32 TotalTorchbearers = 0;

    for (const FWaveSpawnGroup& Group :
        NextWave.SpawnGroups)
    {
        TotalSwordsmen += FMath::Max(
            0, Group.SwordsmanCount);

        TotalMages += FMath::Max(
            0, Group.MageCount);

        TotalTorchbearers += FMath::Max(
            0, Group.TorchbearerCount);
    }

    CurrentWaveIndex = NextIndex;

    AliveEnemies = 0;
    bSpawningFinished = false;

    LevelPhase = ERICLevelPhase::Wave;
    WaveState = EWaveState::Spawning;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d started. "
            "Swordsmen: %d | Mages: %d | "
            "Torchbearers: %d | Groups: %d"),
        GetCurrentWaveNumber(),
        TotalSwordsmen,
        TotalMages,
        TotalTorchbearers,
        NextWave.SpawnGroups.Num());

    OnWaveStarted.Broadcast(
        GetCurrentWaveNumber());
}

// REGISTER ENEMY

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

// ENEMY DIED

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

// FINISH SPAWNING

void AWaveManager::FinishSpawning()
{
    if (WaveState != EWaveState::Spawning)
    {
        return;
    }

    bSpawningFinished = true;
    WaveState = EWaveState::InProgress;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Spawning finished. "
            "Alive enemies: %d"),
        AliveEnemies);

    CheckWaveCompletion();
}

// WAVE REWARD

void AWaveManager::GrantWaveCompletionReward()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this);

    if (!IsValid(Balance))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] Wave reward skipped: GameBalanceManager not found"));
        return;
    }

    const int32 Reward = FMath::Max(
        0,
        Balance->General.WaveCompletionReward
    );

    if (Reward <= 0)
    {
        return;
    }

    APlayerController* PlayerController =
        World->GetFirstPlayerController();

    if (!IsValid(PlayerController))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] Wave reward failed: PlayerController not found"));
        return;
    }

    URICMoneyComponent* MoneyComponent =
        PlayerController->FindComponentByClass<URICMoneyComponent>();

    if (!IsValid(MoneyComponent))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] Wave reward failed: MoneyComponent not found on PlayerController"));
        return;
    }

    const int32 AddedMoney =
        MoneyComponent->AddMoney(Reward);

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d reward | Configured: %d | Added: %d | Balance: %d"),
        GetCurrentWaveNumber(),
        Reward,
        AddedMoney,
        MoneyComponent->CurrentMoney);
}

// CHECK WAVE COMPLETION

void AWaveManager::CheckWaveCompletion()
{
    if (!bSpawningFinished ||
        AliveEnemies > 0 ||
        WaveState != EWaveState::InProgress)
    {
        return;
    }

    TGuardValue<bool> EventGuard(
        bDispatchingWaveEvents, true);

    // Set the completed state first to prevent
    // duplicate rewards from repeated callbacks.
    WaveState = EWaveState::Completed;

    GetWorldTimerManager().ClearTimer(
        NextWaveTimerHandle);

    const bool bLastWave =
        CurrentWaveIndex >= Waves.Num() - 1;

    LevelPhase = bLastWave
        ? ERICLevelPhase::Completed
        : ERICLevelPhase::Wave;

    UE_LOG(LogTemp, Warning,
        TEXT("[WaveManager] Wave %d completed!"),
        GetCurrentWaveNumber());

    // Give the player gold before broadcasting
    // the wave completion event.
    GrantWaveCompletionReward();

    OnWaveCompleted.Broadcast(
        GetCurrentWaveNumber());

    if (bLastWave)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[WaveManager] ALL WAVES COMPLETED!"));

        OnAllWavesCompleted.Broadcast();
        return;
    }

    const float Delay =
        FMath::Max(0.1f, TimeBetweenWaves);

    GetWorldTimerManager().SetTimer(
        NextWaveTimerHandle,
        this,
        &AWaveManager::StartNextWaveInternal,
        Delay,
        false
    );

    UE_LOG(LogTemp, Log,
        TEXT("[WaveManager] Wave %d will start "
            "automatically in %.2f seconds."),
        GetCurrentWaveNumber() + 1,
        Delay);
}

// GETTERS

int32 AWaveManager::GetCurrentWaveNumber() const
{
    return CurrentWaveIndex + 1;
}

int32 AWaveManager::GetTotalWaves() const
{
    return Waves.Num();
}
