
#include "AI/ArcherTowerAI.h"

#include "AI/ProjectileArcher.h"
#include "AI/AIBase.h"

#include "Lighting/DayNightManager.h"
#include "Lighting/Searchlight.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AArcherTowerAI::AArcherTowerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AttackRange = 500.0f;
    DetectionRadius = 500.0f;

    Damage = 10.0f;
    AttackCooldown = 2.5f;
}

void AArcherTowerAI::BeginPlay()
{
    Super::BeginPlay();

    if (!IsValid(DayNightManager.Get()))
    {
        for (TActorIterator<ADayNightManager> It(GetWorld()); It; ++It)
        {
            DayNightManager = *It;
            break;
        }
    }

    if (!IsValid(DayNightManager.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ArcherTower] DayNightManager not found: %s"),
            *GetName());
    }

    NextShotTime = 0.0f;

    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AArcherTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

void AArcherTowerAI::SetProjectileSpawnPoint(
    USceneComponent* NewSpawnPoint)
{
    ProjectileSpawnPoint = NewSpawnPoint;
}

bool AArcherTowerAI::CanFireUnderLighting() const
{
    if (!IsValid(DayNightManager.Get()))
    {
        return false;
    }

    // Daytime: unrestricted firing.
    if (!DayNightManager->IsNight())
    {
        return true;
    }

    const AActor* ActorToCheck =
        IsValid(IlluminationActor.Get())
        ? IlluminationActor.Get()
        : this;

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
    }

    // Nighttime: tower must be illuminated.
    for (TActorIterator<ASearchlight> It(World); It; ++It)
    {
        const ASearchlight* Light = *It;

        if (IsValid(Light) &&
            Light->IsLightActive() &&
            Light->IsActorIlluminated(ActorToCheck))
        {
            return true;
        }
    }

    return false;
}

void AArcherTowerAI::UpdateTower()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(TargetCheckTimer);
        return;
    }

    if (!CanFireUnderLighting())
    {
        Target = nullptr;
        return;
    }

    FindTarget();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextShotTime)
    {
        return;
    }

    if (!CanAttack())
    {
        return;
    }

    Attack();
}

void AArcherTowerAI::Attack()
{
    UWorld* World = GetWorld();

    if (!World || !IsAlive())
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (CurrentTime < NextShotTime)
    {
        return;
    }

    // Check again immediately before firing.
    if (!CanFireUnderLighting())
    {
        return;
    }

    if (!CanAttack())
    {
        return;
    }

    if (!ProjectileClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ArcherTower] ProjectileClass is NULL!"));
        return;
    }

    if (!IsValid(ProjectileSpawnPoint.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ArcherTower] SpawnPoint is NULL!"));
        return;
    }

    AAIBase* EnemyTarget = Target.Get();

    if (!IsValid(EnemyTarget) || !EnemyTarget->IsAlive())
    {
        return;
    }

    const FVector SpawnLocation =
        ProjectileSpawnPoint->GetComponentLocation();

    const FRotator SpawnRotation =
        ProjectileSpawnPoint->GetComponentRotation();

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;

    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AProjectileArcher* Arrow =
        World->SpawnActor<AProjectileArcher>(
            ProjectileClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
        );

    if (!IsValid(Arrow))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ArcherTower] Arrow spawn failed!"));
        return;
    }

    Arrow->InitializeProjectile(EnemyTarget, Damage);

    const float Distance = FVector::Dist(
        SpawnLocation,
        EnemyTarget->GetActorLocation()
    );

    const float FlightTime = FMath::Max(
        Distance / FMath::Max(Arrow->FlightSpeed, 1.0f),
        0.05f
    );

    const float ShotDelay = FMath::Max(
        FMath::Max(AttackCooldown, 0.01f),
        FlightTime * 0.85f
    );

    NextShotTime = CurrentTime + ShotDelay;

    UE_LOG(LogTemp, Log,
        TEXT("[ArcherTower] Shot fired: %s | Delay: %.2f"),
        *GetName(),
        ShotDelay);
}

void AArcherTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);
    Super::EndPlay(EndPlayReason);
}
