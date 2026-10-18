
#include "AI/ArcherTowerAI.h"

#include "Engine/World.h"
#include "TimerManager.h"

// Конструктор
AArcherTowerAI::AArcherTowerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AttackRange = 500.0f;
    DetectionRadius = 500.0f;

    Damage = 10.0f;
    AttackCooldown = 1.0f;
}

// Начало игры
void AArcherTowerAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AArcherTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

// Основной цикл башни
void AArcherTowerAI::UpdateTower()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(TargetCheckTimer);
        return;
    }

    // Обновляем цель с учётом прогресса врагов
    FindTarget();

    if (CanAttack())
    {
        Attack();
    }
}

// Атака
void AArcherTowerAI::Attack()
{
    if (!CanAttack())
    {
        return;
    }

    // Пока используется базовая система нанесения урона.
    // Позднее здесь создадим стрелу через SpawnActor.

    Super::Attack();

    UE_LOG(LogTemp, Warning,
        TEXT("[ArcherTower] Attack executed!"));
}

// Очистка таймера
void AArcherTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);

    Super::EndPlay(EndPlayReason);
}
