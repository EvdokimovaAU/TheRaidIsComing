#include "AI/EnemyAI.h"

#include "AIController.h"

AEnemyAI::AEnemyAI()
{
    PrimaryActorTick.bCanEverTick = false;

    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AEnemyAI::BeginPlay()
{
    Super::BeginPlay();

    if (IsValid(Capitol))
    {
        SetCapitol(Capitol.Get());
    }
}

void AEnemyAI::SetCapitol(AActor* NewCapitol)
{
    Capitol = NewCapitol;
    CurrentTarget = NewCapitol;
}

AActor* AEnemyAI::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

void AEnemyAI::MoveToTarget()
{
    if (!IsValid(CurrentTarget))
    {
        return;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (!EnemyController)
    {
        return;
    }

    EnemyController->MoveToActor(
        CurrentTarget.Get(),
        50.0f
    );
}

void AEnemyAI::StopMovement()
{
    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (EnemyController)
    {
        EnemyController->StopMovement();
    }
}

void AEnemyAI::OnObstacleDetected(AActor* Obstacle)
{
    // Базовая реализация.
    // Конкретное поведение определяют дочерние классы.
}
