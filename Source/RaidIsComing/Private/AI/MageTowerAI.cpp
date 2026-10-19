
#include "AI/MageTowerAI.h"

#include "AI/MageProjectile.h"
#include "AI/AIBase.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ==========================================
// Конструктор
// ==========================================

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

// ==========================================
// Начало игры
// ==========================================

void AMageTowerAI::BeginPlay()
{
    Super::BeginPlay();

    NextShotTime = 0.0f;

    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AMageTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

// ==========================================
// Передача точки выстрела
// ==========================================

void AMageTowerAI::SetProjectileSpawnPoint(
    USceneComponent* NewSpawnPoint)
{
    ProjectileSpawnPoint = NewSpawnPoint;
}

// ==========================================
// Основной цикл башни
// ==========================================

void AMageTowerAI::UpdateTower()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(TargetCheckTimer);
        return;
    }

    // Выбираем противника
    FindTarget();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    // Проверяем кулдаун
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

// ==========================================
// Атака магической башни
// ==========================================

void AMageTowerAI::Attack()
{
    UWorld* World = GetWorld();

    if (!World || !IsAlive())
    {
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    // Защита от повторной атаки
    if (CurrentTime < NextShotTime)
    {
        return;
    }

    if (!CanAttack())
    {
        return;
    }

    // Проверяем класс магического снаряда
    if (!ProjectileClass)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[MageTower] ProjectileClass is NULL!")
        );

        return;
    }

    // Проверяем точку появления снаряда
    if (!IsValid(ProjectileSpawnPoint.Get()))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[MageTower] ProjectileSpawnPoint is NULL!")
        );

        return;
    }

    // Получаем цель
    AAIBase* EnemyTarget = Target.Get();

    if (!IsValid(EnemyTarget) || !EnemyTarget->IsAlive())
    {
        return;
    }

    // ======================================
    // Создание магического снаряда
    // ======================================

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[MageTower] Projectile spawn failed!")
        );

        return;
    }

    // Передаём снаряду цель, урон и радиус взрыва
    Projectile->InitializeProjectile(
        EnemyTarget,
        Damage,
        SplashRadius
    );

    // ======================================
    // Расчёт кулдауна
    // ======================================

    const float Distance = FVector::Dist(
        SpawnLocation,
        EnemyTarget->GetActorLocation()
    );

    const float FlightTime = FMath::Max(
        Distance / FMath::Max(Projectile->FlightSpeed, 1.0f),
        0.05f
    );

    // Следующий выстрел не раньше 85% времени полёта,
    // но с обязательным соблюдением AttackCooldown.
    const float ShotDelay = FMath::Max(
        FMath::Max(AttackCooldown, 0.01f),
        FlightTime * 0.85f
    );

    NextShotTime = CurrentTime + ShotDelay;

    // Синхронизируем кулдаун с базовой системой AI
    LastAttackTime = CurrentTime;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[MAGE DEBUG] Tower: %s | Time: %.2f | Flight: %.2f | Delay: %.2f | Next: %.2f"),
        *GetName(),
        CurrentTime,
        FlightTime,
        ShotDelay,
        NextShotTime
    );

    // Super::Attack() НЕ вызываем.
    // Урон наносится только после попадания магии.
}

// ==========================================
// Завершение работы
// ==========================================

void AMageTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);

    Super::EndPlay(EndPlayReason);
}
