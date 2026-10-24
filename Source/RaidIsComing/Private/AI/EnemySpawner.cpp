
#include "AI/EnemySpawner.h"

#include "AI/WaveManager.h"
#include "AI/EnemyAI.h"

#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"

#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"


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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemySpawner] WaveManager is not assigned!")
        );

        return;
    }

    if (!IsValid(Capitol))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemySpawner] Capitol is not assigned!")
        );

        return;
    }

    WaveManager->OnWaveStarted.AddUniqueDynamic(
        this,
        &AEnemySpawner::HandleWaveStarted
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemySpawner] Connected to WaveManager")
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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemySpawner] Invalid wave index: %d"),
            WaveIndex
        );

        return;
    }

    const FWaveData& WaveData =
        WaveManager->Waves[WaveIndex];

    // Validate all groups before starting.
    for (const FWaveSpawnGroup& Group : WaveData.SpawnGroups)
    {
        const int32 Swordsmen =
            FMath::Max(0, Group.SwordsmanCount);

        const int32 Archers =
            FMath::Max(0, Group.ArcherCount);

        if (Swordsmen + Archers == 0)
        {
            continue;
        }

        if (!IsValid(Group.SpawnPoint))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("[EnemySpawner] Invalid SpawnPoint in wave %d"),
                WaveNumber
            );

            return;
        }

        if ((Swordsmen > 0 && !SwordsmanClass) ||
            (Archers > 0 && !ArcherClass))
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("[EnemySpawner] Missing enemy class in wave %d"),
                WaveNumber
            );

            return;
        }
    }

    if (!IsValid(Capitol))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemySpawner] Capitol is invalid!")
        );

        return;
    }

    GetWorldTimerManager().ClearTimer(SpawnTimer);

    ActiveGroups.Empty();

    CurrentSpawnInterval =
        FMath::Max(0.1f, WaveData.SpawnInterval);

    NextGroupIndex = 0;

    for (const FWaveSpawnGroup& Group : WaveData.SpawnGroups)
    {
        FActiveSpawnGroup ActiveGroup;

        ActiveGroup.SpawnPoint = Group.SpawnPoint;

        ActiveGroup.RemainingSwordsmen =
            FMath::Max(0, Group.SwordsmanCount);

        ActiveGroup.RemainingArchers =
            FMath::Max(0, Group.ArcherCount);

        if (ActiveGroup.HasPendingEnemies())
        {
            ActiveGroups.Add(ActiveGroup);
        }
    }

    bIsSpawning = true;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemySpawner] Wave %d started. Active groups: %d"),
        WaveNumber,
        ActiveGroups.Num()
    );

    SpawnNextEnemy();
}


// ==========================================
// CHECK PENDING ENEMIES
// ==========================================

bool AEnemySpawner::HasPendingEnemies() const
{
    for (const FActiveSpawnGroup& Group : ActiveGroups)
    {
        if (Group.HasPendingEnemies())
        {
            return true;
        }
    }

    return false;
}


// ==========================================
// CHECK DISTANCE FROM EXISTING ENEMIES
// ==========================================

bool AEnemySpawner::IsFarEnoughFromEnemies(
    const FVector& Location
) const
{
    for (TActorIterator<AEnemyAI> It(GetWorld()); It; ++It)
    {
        const AEnemyAI* OtherEnemy = *It;

        if (!IsValid(OtherEnemy) || !OtherEnemy->IsAlive())
        {
            continue;
        }

        const float Distance = FVector::Dist2D(
            Location,
            OtherEnemy->GetActorLocation()
        );

        if (Distance < MinimumSpawnDistance)
        {
            return false;
        }
    }

    return true;
}


// ==========================================
// FIND FREE SPAWN LOCATION
// ==========================================

bool AEnemySpawner::FindFreeSpawnLocation(
    ATargetPoint* Point,
    TSubclassOf<AEnemyAI> EnemyClass,
    FVector& OutLocation
) const
{
    if (!IsValid(Point) || !EnemyClass)
    {
        return false;
    }

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[EnemySpawner] NavigationSystem not found")
        );

        return false;
    }

    const AEnemyAI* EnemyDefault =
        EnemyClass->GetDefaultObject<AEnemyAI>();

    if (!IsValid(EnemyDefault))
    {
        return false;
    }

    const UCapsuleComponent* Capsule =
        EnemyDefault->GetCapsuleComponent();

    if (!Capsule)
    {
        return false;
    }

    const float CapsuleRadius =
        Capsule->GetUnscaledCapsuleRadius();

    const float CapsuleHalfHeight =
        Capsule->GetUnscaledCapsuleHalfHeight();

    const FCollisionShape SpawnShape =
        FCollisionShape::MakeCapsule(
            CapsuleRadius,
            CapsuleHalfHeight
        );

    const FVector Center =
        Point->GetActorLocation();

    const int32 Attempts =
        FMath::Max(1, MaxSpawnAttempts);

    for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
    {
        FVector Candidate = Center;

        if (Attempt > 0)
        {
            const int32 Ring =
                ((Attempt - 1) / 8) + 1;

            const int32 PositionInRing =
                (Attempt - 1) % 8;

            const float RingStep = FMath::Max(
                MinimumSpawnDistance,
                CapsuleRadius * 2.0f + 10.0f
            );

            const float Radius = FMath::Min(
                SpawnSearchRadius,
                Ring * RingStep
            );

            const float Angle =
                (2.0f * PI * PositionInRing / 8.0f) +
                (Ring * 0.35f);

            Candidate.X += FMath::Cos(Angle) * Radius;
            Candidate.Y += FMath::Sin(Angle) * Radius;
        }

        FNavLocation NavLocation;

        if (!NavSys->ProjectPointToNavigation(
            Candidate,
            NavLocation,
            FVector(100.0f, 100.0f, 250.0f)
        ))
        {
            continue;
        }

        const FVector SpawnLocation =
            NavLocation.Location +
            FVector(
                0.0f,
                0.0f,
                CapsuleHalfHeight + 2.0f
            );

        if (FVector::Dist2D(SpawnLocation, Center) >
            SpawnSearchRadius + 1.0f)
        {
            continue;
        }

        if (!IsFarEnoughFromEnemies(SpawnLocation))
        {
            continue;
        }

        const bool bBlocked =
            GetWorld()->OverlapBlockingTestByChannel(
                SpawnLocation,
                FQuat::Identity,
                ECC_Pawn,
                SpawnShape
            );

        if (bBlocked)
        {
            continue;
        }

        OutLocation = SpawnLocation;

        return true;
    }

    return false;
}


// ==========================================
// SPAWN ENEMY FROM GROUP
// ==========================================

bool AEnemySpawner::TrySpawnFromGroup(
    FActiveSpawnGroup& Group
)
{
    if (!Group.HasPendingEnemies())
    {
        return false;
    }

    ATargetPoint* Point = Group.SpawnPoint.Get();

    if (!IsValid(Point) ||
        !IsValid(Capitol) ||
        !IsValid(WaveManager))
    {
        return false;
    }

    const bool bSpawnSwordsman =
        Group.RemainingSwordsmen > 0;

    TSubclassOf<AEnemyAI> ClassToSpawn =
        bSpawnSwordsman
        ? SwordsmanClass
        : ArcherClass;

    if (!ClassToSpawn)
    {
        return false;
    }

    FVector SpawnLocation;

    if (!FindFreeSpawnLocation(
        Point,
        ClassToSpawn,
        SpawnLocation
    ))
    {
        return false;
    }

    FActorSpawnParameters SpawnParams;

    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

    AEnemyAI* Enemy =
        GetWorld()->SpawnActor<AEnemyAI>(
            ClassToSpawn,
            SpawnLocation,
            Point->GetActorRotation(),
            SpawnParams
        );

    if (!IsValid(Enemy))
    {
        return false;
    }

    // Successful spawn.
    if (bSpawnSwordsman)
    {
        --Group.RemainingSwordsmen;
    }
    else
    {
        --Group.RemainingArchers;
    }

    // Register this specific enemy.
    RegisteredEnemies.Add(
        TWeakObjectPtr<AEnemyAI>(Enemy)
    );

    // Subscribe to death before starting movement.
    Enemy->OnEnemyDied.AddUniqueDynamic(
        this,
        &AEnemySpawner::HandleEnemyDied
    );

    // Increase the shared wave counter.
    WaveManager->RegisterEnemy();

    // Assign the Capitol.
    Enemy->SetCapitol(Capitol.Get());

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemySpawner] Spawned %s at %s. Remaining at point: %d"),
        *Enemy->GetName(),
        *Point->GetName(),
        Group.RemainingSwordsmen + Group.RemainingArchers
    );

    return true;
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

    if (!HasPendingEnemies())
    {
        CompleteSpawning();
        return;
    }

    bool bSpawnSuccessful = false;

    const int32 GroupCount = ActiveGroups.Num();

    for (int32 Attempt = 0; Attempt < GroupCount; ++Attempt)
    {
        const int32 GroupIndex =
            (NextGroupIndex + Attempt) % GroupCount;

        FActiveSpawnGroup& Group =
            ActiveGroups[GroupIndex];

        if (!Group.HasPendingEnemies())
        {
            continue;
        }

        if (TrySpawnFromGroup(Group))
        {
            NextGroupIndex =
                (GroupIndex + 1) % GroupCount;

            bSpawnSuccessful = true;

            break;
        }
    }

    if (!HasPendingEnemies())
    {
        CompleteSpawning();
        return;
    }

    const float NextDelay =
        bSpawnSuccessful
        ? CurrentSpawnInterval
        : FMath::Max(0.1f, RetryInterval);

    if (!bSpawnSuccessful)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemySpawner] Spawn points blocked. Retrying...")
        );
    }

    GetWorldTimerManager().SetTimer(
        SpawnTimer,
        this,
        &AEnemySpawner::SpawnNextEnemy,
        NextDelay,
        false
    );
}


// ==========================================
// ENEMY DIED
// ==========================================

void AEnemySpawner::HandleEnemyDied(
    AEnemyAI* DeadEnemy
)
{
    if (!IsValid(DeadEnemy))
    {
        return;
    }

    // A death is counted only once.
    if (RegisteredEnemies.Remove(
        TWeakObjectPtr<AEnemyAI>(DeadEnemy)
    ) == 0)
    {
        return;
    }

    DeadEnemy->OnEnemyDied.RemoveDynamic(
        this,
        &AEnemySpawner::HandleEnemyDied
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemySpawner] Enemy death received: %s"),
        *DeadEnemy->GetName()
    );

    if (IsValid(WaveManager))
    {
        WaveManager->NotifyEnemyDied();
    }
}


// ==========================================
// COMPLETE SPAWNING
// ==========================================

void AEnemySpawner::CompleteSpawning()
{
    if (!bIsSpawning)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(SpawnTimer);

    bIsSpawning = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[EnemySpawner] All configured enemies spawned")
    );

    if (IsValid(WaveManager))
    {
        WaveManager->FinishSpawning();
    }
}


// ==========================================
// END PLAY
// ==========================================

void AEnemySpawner::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    GetWorldTimerManager().ClearTimer(SpawnTimer);

    for (const TWeakObjectPtr<AEnemyAI>& EnemyPtr : RegisteredEnemies)
    {
        AEnemyAI* Enemy = EnemyPtr.Get();

        if (IsValid(Enemy))
        {
            Enemy->OnEnemyDied.RemoveDynamic(
                this,
                &AEnemySpawner::HandleEnemyDied
            );
        }
    }

    RegisteredEnemies.Empty();

    if (IsValid(WaveManager))
    {
        WaveManager->OnWaveStarted.RemoveDynamic(
            this,
            &AEnemySpawner::HandleWaveStarted
        );
    }

    Super::EndPlay(EndPlayReason);
}
