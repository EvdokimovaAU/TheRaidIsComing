
#include "AI/EnemySpawner.h"

#include "AI/WaveManager.h"
#include "AI/EnemyAI.h"
#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "EnemyCombatNavigation.h"
#include "Construction/RICBuilding.h"
#include "NavigationPath.h"
#include "AI/GameBalanceManager.h"

#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"

// CONSTRUCTOR

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

// BALANCE

void AEnemySpawner::ApplySpawnBalance()
{
    AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this);

    if (!IsValid(Balance))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemySpawner] BalanceManager not found. Using BP settings."));
        return;
    }

    const FSpawnBalanceSettings& Settings =
        Balance->Spawning;

    // Compact spawning. Balance settings cannot
    // force enemies to appear far apart.
    MinimumSpawnDistance = FMath::Clamp(
        Settings.MinimumSpawnDistance, 0.f, 15.f);

    SpawnSearchRadius = FMath::Clamp(
        Settings.SpawnSearchRadius, 100.f, 300.f);

    MaxSpawnAttempts = FMath::Max(
        64, Settings.MaxSpawnAttempts);

    RetryInterval = FMath::Max(
        0.01f, Settings.RetryInterval);

    UE_LOG(LogTemp, Log,
        TEXT("[EnemySpawner] Balance | Gap=%.1f Radius=%.1f Attempts=%d"),
        MinimumSpawnDistance,
        SpawnSearchRadius,
        MaxSpawnAttempts);
}

// BEGIN PLAY

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    ApplySpawnBalance();

    if (!IsValid(WaveManager))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] WaveManager not assigned"));
        return;
    }

    if (!IsValid(Capitol))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Capitol not assigned"));
        return;
    }

    WaveManager->OnWaveStarted.AddUniqueDynamic(
        this,
        &AEnemySpawner::HandleWaveStarted
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Connected to WaveManager"));
}

// WAVE STARTED

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

    if (!IsValid(Capitol))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] Capitol invalid"));
        return;
    }

    for (const FWaveSpawnGroup& Group :
        WaveData.SpawnGroups)
    {
        const int32 Swordsmen =
            FMath::Max(0, Group.SwordsmanCount);

        const int32 Mages =
            FMath::Max(0, Group.MageCount);

        const int32 Torchbearers =
            FMath::Max(0, Group.TorchbearerCount);

        if (Swordsmen + Mages + Torchbearers == 0)
        {
            continue;
        }

        if (!IsValid(Group.SpawnPoint))
        {
            UE_LOG(LogTemp, Error,
                TEXT("[EnemySpawner] Missing spawn point"));
            return;
        }

        if (Swordsmen > 0 && !SwordsmanClass)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[EnemySpawner] SwordsmanClass missing"));
            return;
        }

        if (Mages > 0 && !MageClass)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[EnemySpawner] MageClass missing"));
            return;
        }

        if (Torchbearers > 0 && !TorchbearerClass)
        {
            UE_LOG(LogTemp, Error,
                TEXT("[EnemySpawner] TorchbearerClass missing"));
            return;
        }
    }

    GetWorldTimerManager().ClearTimer(SpawnTimer);

    ActiveGroups.Empty();
    NextGroupIndex = 0;

    CurrentSpawnInterval =
        FMath::Max(0.1f, WaveData.SpawnInterval);

    for (const FWaveSpawnGroup& Group :
        WaveData.SpawnGroups)
    {
        FActiveSpawnGroup ActiveGroup;

        ActiveGroup.SpawnPoint = Group.SpawnPoint;

        ActiveGroup.RemainingSwordsmen =
            FMath::Max(0, Group.SwordsmanCount);

        ActiveGroup.RemainingMages =
            FMath::Max(0, Group.MageCount);

        ActiveGroup.RemainingTorchbearers =
            FMath::Max(0, Group.TorchbearerCount);

        if (ActiveGroup.HasPendingEnemies())
        {
            ActiveGroups.Add(ActiveGroup);
        }
    }

    bIsSpawning = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Wave %d | Active groups: %d"),
        WaveNumber,
        ActiveGroups.Num());

    SpawnNextEnemy();
}

// PENDING ENEMIES

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

// CAPSULE DISTANCE CHECK

bool AEnemySpawner::IsFarEnoughFromEnemies(
    const FVector& Location,
    float NewCapsuleRadius,
    float NewCapsuleHalfHeight
) const
{
    if (!GetWorld())
    {
        return false;
    }

    for (TActorIterator<AEnemyAI> It(GetWorld()); It; ++It)
    {
        const AEnemyAI* Other = *It;

        if (!IsValid(Other) || !Other->IsAlive())
        {
            continue;
        }

        const UCapsuleComponent* OtherCapsule =
            Other->GetCapsuleComponent();

        if (!OtherCapsule)
        {
            continue;
        }

        const float OtherRadius =
            OtherCapsule->GetScaledCapsuleRadius();

        const float OtherHalfHeight =
            OtherCapsule->GetScaledCapsuleHalfHeight();

        const float RequiredDistance =
            NewCapsuleRadius +
            OtherRadius +
            MinimumSpawnDistance;

        const float HorizontalDistance =
            FVector::Dist2D(
                Location,
                Other->GetActorLocation()
            );

        const float VerticalDistance =
            FMath::Abs(
                Location.Z -
                Other->GetActorLocation().Z
            );

        // Ignore enemies on a sufficiently
        // different vertical level.
        if (VerticalDistance >=
            NewCapsuleHalfHeight + OtherHalfHeight)
        {
            continue;
        }

        if (HorizontalDistance < RequiredDistance)
        {
            return false;
        }
    }

    return true;
}

// FIND COMPACT SPAWN LOCATION

bool AEnemySpawner::FindFreeSpawnLocation(
    ATargetPoint* Point,
    TSubclassOf<AEnemyAI> EnemyClass,
    FVector& OutLocation,
    AEnemyAI* Companion
) const
{
    if (!GetWorld() || !IsValid(Point) || !EnemyClass)
    {
        return false;
    }

    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld());

    if (!NavSys)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemySpawner] NavigationSystem missing"));
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
        Capsule->GetScaledCapsuleRadius();

    const float CapsuleHalfHeight =
        Capsule->GetScaledCapsuleHalfHeight();

    const FVector Center =
        Point->GetActorLocation();

    const int32 Attempts =
        FMath::Max(1, MaxSpawnAttempts);

    const float Gap =
        FMath::Max(5.f, MinimumSpawnDistance);

    // Conservative step for a compact grid.
    const float RingStep =
        CapsuleRadius * 2.f + Gap;

    const FCollisionShape SpawnShape =
        FCollisionShape::MakeCapsule(
            CapsuleRadius,
            CapsuleHalfHeight
        );

    FVector CompanionForward = FVector::ZeroVector;
    if (IsValid(Companion))
    {
        FVector Approach;
        if (EnemyCombatNavigation::FindApproach(Companion, Cast<ARICBuilding>(Capitol.Get()),
            Companion->AttackRange, Approach))
        {
            auto* Route = NavSys->FindPathToLocationSynchronously(GetWorld(),
                Companion->GetActorLocation(), Approach, Companion);
            if (IsValid(Route) && Route->PathPoints.Num() > 1)
                CompanionForward = (Route->PathPoints[1] - Route->PathPoints[0]).GetSafeNormal2D();
        }
    }

    for (int32 Attempt = 0;
        Attempt < Attempts;
        ++Attempt)
    {
        FVector Candidate = Center;

        if (Attempt > 0)
        {
            // First ring: 8 positions.
            // Next rings: 16, 24, 32, etc.
            int32 RemainingSlot = Attempt - 1;
            int32 Ring = 1;
            int32 SlotsInRing = 8;

            while (RemainingSlot >= SlotsInRing)
            {
                RemainingSlot -= SlotsInRing;
                ++Ring;
                SlotsInRing = Ring * 8;
            }

            const float Radius =
                Ring * RingStep;

            if (Radius > SpawnSearchRadius)
            {
                continue;
            }

            const float Angle =
                (2.f * PI * RemainingSlot /
                    SlotsInRing) +
                Ring * 0.15f;

            Candidate.X +=
                FMath::Cos(Angle) * Radius;

            Candidate.Y +=
                FMath::Sin(Angle) * Radius;
        }

        FNavLocation NavLocation;

        if (!NavSys->ProjectPointToNavigation(
            Candidate,
            NavLocation,
            FVector(60.f, 60.f, 250.f)))
        {
            continue;
        }

        // Navigation gives the ground position.
        // ACharacter needs the capsule center.
        const FVector SpawnLocation =
            NavLocation.Location +
            FVector(
                0.f,
                0.f,
                CapsuleHalfHeight + 2.f
            );

        if (FVector::Dist2D(
            SpawnLocation, Center) >
            SpawnSearchRadius + 1.f)
        {
            continue;
        }

        if (!IsFarEnoughFromEnemies(
            SpawnLocation,
            CapsuleRadius,
            CapsuleHalfHeight))
        {
            continue;
        }

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        const bool bBlocked =
            GetWorld()->OverlapBlockingTestByChannel(
                SpawnLocation,
                FQuat::Identity,
                ECC_Pawn,
                SpawnShape,
                QueryParams
            );

        if (bBlocked)
        {
            continue;
        }

        if (IsValid(Companion))
        {
            // Place the mage behind or beside the swordsman on its actual
            // route. A mage in front cannot be overtaken in a narrow lane.
            if (FVector::DotProduct(NavLocation.Location - Companion->GetActorLocation(),
                CompanionForward) > CapsuleRadius * 0.25f) continue;
            const auto* MageDefault = Cast<AEnemyMageAI>(EnemyDefault);
            const auto* DomeDefault = MageDefault && MageDefault->BarrierClass
                ? MageDefault->BarrierClass->GetDefaultObject<AEnemyBarrier>() : nullptr;
            const float CoverRadius = DomeDefault ? DomeDefault->BarrierRadius : 180.f;
            auto* Path = NavSys->FindPathToLocationSynchronously(GetWorld(),
                Companion->GetActorLocation(), NavLocation.Location, Companion);
            if (!IsValid(Path) || !Path->IsValid() || Path->IsPartial() ||
                Path->GetPathLength() > CoverRadius - CapsuleRadius) continue;
        }
        OutLocation = SpawnLocation;
        return true;
    }

    return false;
}

// SPAWN ONE ENEMY

bool AEnemySpawner::SpawnEnemy(
    FActiveSpawnGroup& Group,
    TSubclassOf<AEnemyAI> EnemyClass,
    int32& RemainingCount,
    AEnemyAI** OutEnemy
)
{
    if (RemainingCount <= 0 ||
        !EnemyClass ||
        !IsValid(WaveManager) ||
        !IsValid(Capitol))
    {
        return false;
    }

    ATargetPoint* Point = Group.SpawnPoint.Get();

    if (!IsValid(Point))
    {
        return false;
    }

    FVector SpawnLocation;

    if (!FindFreeSpawnLocation(
        Point,
        EnemyClass,
        SpawnLocation,
        EnemyClass->IsChildOf(AEnemyMageAI::StaticClass()) ? Group.PendingCompanion.Get() : nullptr))
    {
        return false;
    }

    FActorSpawnParameters Params;

    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::
        DontSpawnIfColliding;

    AEnemyAI* Enemy =
        GetWorld()->SpawnActor<AEnemyAI>(
            EnemyClass,
            SpawnLocation,
            (Capitol->GetActorLocation() - SpawnLocation).GetSafeNormal2D().Rotation(),
            Params
        );

    if (!IsValid(Enemy))
    {
        return false;
    }

    if (OutEnemy) *OutEnemy = Enemy;
    --RemainingCount;

    RegisteredEnemies.Add(
        TWeakObjectPtr<AEnemyAI>(Enemy)
    );

    Enemy->OnEnemyDied.AddUniqueDynamic(
        this,
        &AEnemySpawner::HandleEnemyDied
    );

    WaveManager->RegisterEnemy();

    Enemy->SetCapitol(Capitol.Get());
    EnemyCombatNavigation::FaceTarget(Enemy, Capitol.Get());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Spawned %s at %s | Location: %s"),
        *Enemy->GetName(),
        *Point->GetName(),
        *SpawnLocation.ToString());

    return true;
}

// SPAWN FROM GROUP

bool AEnemySpawner::TrySpawnFromGroup(
    FActiveSpawnGroup& Group
)
{
    if (!Group.HasPendingEnemies())
    {
        return false;
    }

    if (!Group.SpawnPoint.IsValid())
    {
        return false;
    }

    // A pending pair is completed before spawning another swordsman. Both
    // members are bound in the same scheduler call, before any behavior timer.
    if (Group.RemainingMages > 0 && (Group.PendingCompanion.IsValid() || Group.RemainingSwordsmen > 0))
    {
        bool bSpawnedSword = false;
        if (!Group.PendingCompanion.IsValid())
        {
            AEnemyAI* Sword = nullptr;
            if (!SpawnEnemy(Group, SwordsmanClass, Group.RemainingSwordsmen, &Sword)) return false;
            Group.PendingCompanion = Sword;
            bSpawnedSword = true;
        }
        AEnemyAI* MageActor = nullptr;
        if (SpawnEnemy(Group, MageClass, Group.RemainingMages, &MageActor))
        {
            if (auto* Mage = Cast<AEnemyMageAI>(MageActor))
                Mage->BindSpawnCompanion(Cast<AEnemySwordsmanAI>(Group.PendingCompanion.Get()));
            Group.PendingCompanion.Reset();
            return true;
        }
        return bSpawnedSword;
    }

    // Spawn any remaining swordsmen.
    if (Group.RemainingSwordsmen > 0)
    {
        return SpawnEnemy(
            Group,
            SwordsmanClass,
            Group.RemainingSwordsmen
        );
    }

    // Spawn any remaining mages.
    if (Group.RemainingMages > 0)
    {
        return SpawnEnemy(
            Group,
            MageClass,
            Group.RemainingMages
        );
    }

    // Spawn torchbearers normally.
    if (Group.RemainingTorchbearers > 0)
    {
        return SpawnEnemy(
            Group,
            TorchbearerClass,
            Group.RemainingTorchbearers
        );
    }

    return false;
}

// SPAWN NEXT ENEMY

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

    const int32 GroupCount =
        ActiveGroups.Num();

    for (int32 Attempt = 0;
        Attempt < GroupCount;
        ++Attempt)
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
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemySpawner] Spawn blocked. Retrying"));
    }

    GetWorldTimerManager().SetTimer(
        SpawnTimer,
        this,
        &AEnemySpawner::SpawnNextEnemy,
        NextDelay,
        false
    );
}

// ENEMY DIED

void AEnemySpawner::HandleEnemyDied(
    AEnemyAI* DeadEnemy
)
{
    if (!IsValid(DeadEnemy))
    {
        return;
    }

    if (RegisteredEnemies.Remove(
        TWeakObjectPtr<AEnemyAI>(DeadEnemy)) == 0)
    {
        return;
    }

    DeadEnemy->OnEnemyDied.RemoveDynamic(
        this,
        &AEnemySpawner::HandleEnemyDied
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] Enemy died: %s"),
        *DeadEnemy->GetName());

    if (IsValid(WaveManager))
    {
        WaveManager->NotifyEnemyDied();
    }
}

// COMPLETE SPAWNING

void AEnemySpawner::CompleteSpawning()
{
    if (!bIsSpawning)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(
        SpawnTimer);

    bIsSpawning = false;

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemySpawner] All enemies spawned"));

    if (IsValid(WaveManager))
    {
        WaveManager->FinishSpawning();
    }
}

// END PLAY

void AEnemySpawner::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    GetWorldTimerManager().ClearTimer(
        SpawnTimer);

    for (const TWeakObjectPtr<AEnemyAI>& EnemyPtr :
        RegisteredEnemies)
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
