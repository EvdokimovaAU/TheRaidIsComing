
#include "AI/EnemySwordsmanAI.h"

#include "TimerManager.h"
#include "Engine/World.h"

// Конструктор
AEnemySwordsmanAI::AEnemySwordsmanAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

// Реакция на препятствие
void AEnemySwordsmanAI::OnObstacleDetected(AActor* Obstacle)
{
    if (!IsValid(Obstacle))
    {
        return;
    }

    // Останавливаем движение
    StopMovement();

    // Начинаем атаковать препятствие
    StartWallAttack(Obstacle);
}

// Начало атаки стены
void AEnemySwordsmanAI::StartWallAttack(AActor* Wall)
{
    if (!IsValid(Wall))
    {
        return;
    }

    StopWallAttack();

    TargetWall = Wall;

    GetWorldTimerManager().SetTimer(
        WallAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackWall,
        FMath::Max(WallAttackCooldown, 0.01f),
        true
    );
}

// Один удар по стене
void AEnemySwordsmanAI::AttackWall()
{
    if (!IsValid(TargetWall))
    {
        OnWallDestroyed();
        return;
    }

    // Здесь позднее вызовем систему получения урона стены.
    // Например, через интерфейс разрушаемых объектов.
}

// Остановка атаки
void AEnemySwordsmanAI::StopWallAttack()
{
    GetWorldTimerManager().ClearTimer(WallAttackTimer);

    TargetWall = nullptr;
}

// Стена разрушена
void AEnemySwordsmanAI::OnWallDestroyed()
{
    StopWallAttack();

    // Повторно запрашиваем маршрут до Капитолия
    MoveToTarget();
}
