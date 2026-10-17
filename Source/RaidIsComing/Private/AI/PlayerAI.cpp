#include "AI/PlayerAI.h"
#include "AI/EnemyAI.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

// Конструктор
APlayerAI::APlayerAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

// Начало игры
void APlayerAI::BeginPlay()
{
    Super::BeginPlay();
}

// Поиск врагов в радиусе
TArray<AEnemyAI*> APlayerAI::FindEnemiesInRadius()
{
    TArray<AEnemyAI*> Enemies;

    if (!GetWorld())
    {
        return Enemies;
    }

    TArray<FOverlapResult> Results;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Results,
        GetActorLocation(),
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(DetectionRadius),
        QueryParams
    );

    for (const FOverlapResult& Result : Results)
    {
        AEnemyAI* Enemy = Cast<AEnemyAI>(Result.GetActor());

        if (IsValid(Enemy) && Enemy->IsAlive())
        {
            Enemies.AddUnique(Enemy);
        }
    }

    return Enemies;
}

// Выбор приоритетной цели
AEnemyAI* APlayerAI::SelectTarget(
    const TArray<AEnemyAI*>& Enemies)
{
    AEnemyAI* BestTarget = nullptr;

    for (AEnemyAI* Enemy : Enemies)
    {
        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        if (!BestTarget)
        {
            BestTarget = Enemy;
            continue;
        }

        // Кто дальше продвинулся по маршруту?
        if (Enemy->RouteProgress > BestTarget->RouteProgress)
        {
            BestTarget = Enemy;
        }
        // При одинаковом прогрессе выбираем меньший ID
        else if (
            FMath::IsNearlyEqual(
                Enemy->RouteProgress,
                BestTarget->RouteProgress
            )
            && Enemy->ID < BestTarget->ID
            )
        {
            BestTarget = Enemy;
        }
    }

    return BestTarget;
}

// Поиск и сохранение цели
void APlayerAI::FindTarget()
{
    TArray<AEnemyAI*> Enemies = FindEnemiesInRadius();

    Target = SelectTarget(Enemies);
}

// Реакция на смерть цели
void APlayerAI::OnTargetDied()
{
    Target = nullptr;

    FindTarget();
}
