
#include "AI/MageTowerAI.h"

#include "AI/MageProjectile.h"
#include "AI/AIBase.h"

#include "Lighting/DayNightManager.h"
#include "Lighting/Searchlight.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AMageTowerAI::AMageTowerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AttackRange = 450.0f;
    DetectionRadius = 450.0f;

    Damage = 20.0f;
    AttackCooldown = 3.0f;

    SplashRadius = 200.0f;
    TargetCheckInterval = 0.2f;
}

void AMageTowerAI::BeginPlay()
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
            TEXT("[MageTower] DayNightManager not found: %s"),
            *GetName());
    }

    NextShotTime = 0.0f;

    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AMageTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

void AMageTowerAI::SetProjectileSpawnPoint(
    USceneComponent* NewSpawnPoint)
{
    ProjectileSpawnPoint = NewSpawnPoint;
}

bool AMageTowerAI::CanFireUnderLighting() const
{
    if (!IsValid(DayNightManager.Get()))
    {
        return false;
    }

    // Day: normal tower behavior.
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

    // Night: require an active searchlight covering the tower.
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

void AMageTowerAI::UpdateTower()
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

void AMageTowerAI::Attack()
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

    // Prevent firing if the searchlight moved away.
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
            TEXT("[MageTower] ProjectileClass is NULL!"));
        return;
    }

    if (!IsValid(ProjectileSpawnPoint.Get()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[MageTower] ProjectileSpawnPoint is NULL!"));
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

    AMageProjectile* Projectile =
        World->SpawnActor<AMageProjectile>(
            ProjectileClass,
            SpawnLocation,
            SpawnRotation,
            SpawnParams
        );

    if (!IsValid(Projectile))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[MageTower] Projectile spawn failed!"));
        return;
    }

    // Keep existing splash damage mechanics.
    Projectile->InitializeProjectile(
        EnemyTarget,
        Damage,
        SplashRadius
    );

    const float Distance = FVector::Dist(
        SpawnLocation,
        EnemyTarget->GetActorLocation()
    );

    const float FlightTime = FMath::Max(
        Distance / FMath::Max(Projectile->FlightSpeed, 1.0f),
        0.05f
    );

    const float ShotDelay = FMath::Max(
        FMath::Max(AttackCooldown, 0.01f),
        FlightTime * 0.85f
    );

    NextShotTime = CurrentTime + ShotDelay;

    // Synchronize with base AI attack timing.
    LastAttackTime = CurrentTime;

    UE_LOG(LogTemp, Log,
        TEXT("[MageTower] Shot fired: %s | Delay: %.2f"),
        *GetName(),
        ShotDelay);
}

void AMageTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);

    Super::EndPlay(EndPlayReason);
}
