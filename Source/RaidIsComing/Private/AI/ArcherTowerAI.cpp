
#include "AI/ArcherTowerAI.h"

#include "AI/ProjectileArcher.h"
#include "AI/AIBase.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// ==========================================
// Конструктор
// ==========================================

AArcherTowerAI::AArcherTowerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    // Радиус обнаружения и атаки
    AttackRange = 500.0f;
    DetectionRadius = 500.0f;

    // Урон одной стрелы
    Damage = 10.0f;

    // Минимальная задержка между выстрелами
    AttackCooldown = 2.5f;
}

// ==========================================
// Начало игры
// ==========================================

void AArcherTowerAI::BeginPlay()
{
    Super::BeginPlay();

    NextShotTime = 0.0f;

    // Проверяем противников каждые 0.2 секунды
    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AArcherTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

// ==========================================
// Передача точки появления стрелы
// ==========================================

void AArcherTowerAI::SetProjectileSpawnPoint(
    USceneComponent* NewSpawnPoint)
{
    ProjectileSpawnPoint = NewSpawnPoint;
}

// ==========================================
// Основной цикл башни
// ==========================================

void AArcherTowerAI::UpdateTower()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(TargetCheckTimer);
        return;
    }

    // Обновляем приоритетную цель
    FindTarget();

    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime =
        GetWorld()->GetTimeSeconds();

    // Кулдаун ещё не закончился
    if (CurrentTime < NextShotTime)
    {
        return;
    }

    // Проверяем базовые условия атаки
    if (!CanAttack())
    {
        return;
    }

    Attack();
}

// ==========================================
// Атака башни
// ==========================================

void AArcherTowerAI::Attack()
{
    UWorld* World = GetWorld();

    if (!World || !IsAlive())
    {
        return;
    }

    const float CurrentTime =
        World->GetTimeSeconds();

    // Повторная защита от слишком частых атак
    if (CurrentTime < NextShotTime)
    {
        return;
    }

    if (!CanAttack())
    {
        return;
    }

    // Проверяем Blueprint стрелы
    if (!ProjectileClass)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[ArcherTower] ProjectileClass is NULL!")
        );

        return;
    }

    // Проверяем точку выстрела
    if (!IsValid(ProjectileSpawnPoint.Get()))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[ArcherTower] SpawnPoint is NULL!")
        );

        return;
    }

    // Получаем текущую цель
    AAIBase* EnemyTarget = Target.Get();

    if (!IsValid(EnemyTarget) || !EnemyTarget->IsAlive())
    {
        return;
    }

    // ======================================
    // Создание стрелы
    // ======================================

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
        UE_LOG(
            LogTemp,
            Error,
            TEXT("[ArcherTower] Arrow spawn failed!")
        );

        return;
    }

    // Передаём стреле противника и урон
    Arrow->InitializeProjectile(
        EnemyTarget,
        Damage
    );

    // ======================================
    // Расчёт задержки между выстрелами
    // ======================================

    // Расстояние от башни до врага
    const float Distance = FVector::Dist(
        SpawnLocation,
        EnemyTarget->GetActorLocation()
    );

    // Примерное время полёта стрелы
    const float FlightTime = FMath::Max(
        Distance / FMath::Max(Arrow->FlightSpeed, 1.0f),
        0.05f
    );

    // Следующая стрела не должна появляться,
    // пока предыдущая не пройдёт 85% времени полёта.
    // Также соблюдаем минимальный AttackCooldown.
    const float ShotDelay = FMath::Max(
        FMath::Max(AttackCooldown, 0.01f),
        FlightTime * 0.85f
    );

    // Фиксируем время следующего выстрела
    NextShotTime = CurrentTime + ShotDelay;

    // ======================================
    // Отладка
    // ======================================

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[ARROW DEBUG] Tower: %s | Time: %.2f | Flight: %.2f | Delay: %.2f | Next: %.2f"),
        *GetName(),
        CurrentTime,
        FlightTime,
        ShotDelay,
        NextShotTime
    );

    // ВАЖНО:
    // Super::Attack() НЕ вызываем.
    // Урон наносится самой стрелой при попадании.
}

// ==========================================
// Завершение работы
// ==========================================

void AArcherTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);

    Super::EndPlay(EndPlayReason);
}
