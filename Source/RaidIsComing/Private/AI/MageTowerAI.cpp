
#include "AI/MageTowerAI.h"
#include "AI/EnemyAI.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// Конструктор
AMageTowerAI::AMageTowerAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AttackRange = 450.0f;
    DetectionRadius = 450.0f;

    Damage = 20.0f;
    AttackCooldown = 3.0f;
}

// Начало игры
void AMageTowerAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        TargetCheckTimer,
        this,
        &AMageTowerAI::UpdateTower,
        FMath::Max(TargetCheckInterval, 0.01f),
        true
    );
}

// Основной цикл башни
void AMageTowerAI::UpdateTower()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(TargetCheckTimer);
        return;
    }

    FindTarget();

    if (CanAttack())
    {
        Attack();
    }
}

// Магическая атака
void AMageTowerAI::Attack()
{
    if (!CanAttack())
    {
        return;
    }

    // Запоминаем положение цели
    const FVector ImpactLocation = Target->GetActorLocation();

    // Обновляем время последней атаки
    LastAttackTime = GetWorld()->GetTimeSeconds();

    // Наносим массовый урон
    ApplySplashDamage(ImpactLocation);
}

// Массовый урон
void AMageTowerAI::ApplySplashDamage(
    const FVector& ImpactLocation)
{
    if (!GetWorld())
    {
        return;
    }

    // Перебираем вражеских юнитов
    for (TActorIterator<AEnemyAI> It(GetWorld()); It; ++It)
    {
        AEnemyAI* Enemy = *It;

        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        // Расстояние от центра попадания
        const float Distance = FVector::Dist(
            ImpactLocation,
            Enemy->GetActorLocation()
        );

        // Проверяем попадание в радиус
        if (Distance <= SplashRadius)
        {
            Enemy->ReceiveDamage(Damage);
        }
    }
}

// Очистка таймера
void AMageTowerAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TargetCheckTimer);

    Super::EndPlay(EndPlayReason);
}
