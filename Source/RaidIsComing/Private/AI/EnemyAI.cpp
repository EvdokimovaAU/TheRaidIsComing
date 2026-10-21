
#include "AI/EnemyAI.h"

#include "AIController.h"

#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"


// КОНСТРУКТОР

AEnemyAI::AEnemyAI()
{
    PrimaryActorTick.bCanEverTick = false;

    // Контроллер ИИ
    AIControllerClass = AAIController::StaticClass();

    AutoPossessAI =
        EAutoPossessAI::PlacedInWorldOrSpawned;

    // Начальная скорость
    GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}


// НАЧАЛО ИГРЫ

void AEnemyAI::BeginPlay()
{
    Super::BeginPlay();

    // Применяем скорость из Blueprint
    GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;

    // Назначаем капитолий
    if (IsValid(Capitol))
    {
        SetCapitol(Capitol.Get());
    }

    if (bAutoStartMovement)
    {
        GetWorldTimerManager().SetTimer(
            MovementStartTimer,
            this,
            &AEnemyAI::TryStartMovement,
            0.2f,
            true
        );

        TryStartMovement();
    }
}


// ОЖИДАНИЕ ГОТОВНОСТИ ИИ

void AEnemyAI::TryStartMovement()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(
            MovementStartTimer
        );

        return;
    }

    // Цель может быть назначена позже
    if (!IsValid(CurrentTarget))
    {
        return;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    // Ожидаем получения контроллера
    if (!EnemyController)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    MoveToTarget();
}


// НАЗНАЧЕНИЕ КАПИТОЛИЯ

void AEnemyAI::SetCapitol(AActor* NewCapitol)
{
    if (!IsValid(NewCapitol) || NewCapitol == this)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Invalid Capitol"));

        return;
    }

    Capitol = NewCapitol;
    CurrentTarget = NewCapitol;

    UE_LOG(LogTemp, Log,
        TEXT("[EnemyAI] %s: Capitol assigned: %s"),
        *GetName(),
        *NewCapitol->GetName());

    if (bAutoStartMovement && HasActorBegunPlay())
    {
        TryStartMovement();
    }
}


// ПОЛУЧЕНИЕ ТЕКУЩЕЙ ЦЕЛИ

AActor* AEnemyAI::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}


// ПЕРЕМЕЩЕНИЕ К ЦЕЛИ

void AEnemyAI::MoveToTarget()
{
    if (!IsAlive() || !IsValid(CurrentTarget))
    {
        return;
    }

    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (!EnemyController)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] AIController not found"));

        return;
    }

    // Получаем систему навигации
    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(
            GetWorld()
        );

    if (!NavSys)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] NavigationSystem not found"));

        return;
    }

    const FVector EnemyLocation = GetActorLocation();

    const FVector TargetLocation =
        CurrentTarget->GetActorLocation();


    // ПРОВЕРКА ПОЛОЖЕНИЯ ВРАГА

    FNavLocation EnemyNavLocation;

    const bool bEnemyOnNavMesh =
        NavSys->ProjectPointToNavigation(
            EnemyLocation,
            EnemyNavLocation,
            FVector(150.f, 150.f, 250.f)
        );

    if (!bEnemyOnNavMesh)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] Enemy is outside NavMesh"));

        return;
    }


    // ПОИСК ДОСТУПНОЙ ТОЧКИ ОКОЛО ЦЕЛИ

    FNavLocation TargetNavLocation;

    const bool bTargetOnNavMesh =
        NavSys->ProjectPointToNavigation(
            TargetLocation,
            TargetNavLocation,
            FVector(
                NavigationSearchRadius,
                NavigationSearchRadius,
                300.f
            )
        );

    if (!bTargetOnNavMesh)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] No NavMesh near target: %s"),
            *CurrentTarget->GetName());

        return;
    }


    // ПРОВЕРКА МАРШРУТА

    UNavigationPath* Path =
        NavSys->FindPathToLocationSynchronously(
            GetWorld(),
            EnemyNavLocation.Location,
            TargetNavLocation.Location,
            this
        );

    if (!IsValid(Path) ||
        !Path->IsValid() ||
        Path->IsPartial())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Complete path not found"));

        return;
    }


    // ДИАГНОСТИКА

    UE_LOG(LogTemp, Warning,
        TEXT("========== ENEMY MOVEMENT =========="));

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy: %s"),
        *GetName());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Target: %s"),
        *CurrentTarget->GetName());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Destination: %s"),
        *TargetNavLocation.Location.ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Movement speed: %.2f"),
        GetCharacterMovement()->MaxWalkSpeed);


    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Enemy position: %s"),
        *GetActorLocation().ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Capitol position: %s"),
        *CurrentTarget->GetActorLocation().ToString());

    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyAI] Navigation destination: %s"),
        *TargetNavLocation.Location.ToString());

    // ЗАПУСК ПЕРЕМЕЩЕНИЯ

    const EPathFollowingRequestResult::Type Result =
        EnemyController->MoveToLocation(
            TargetNavLocation.Location,
            AcceptanceRadius,
            true,
            true,
            false,
            true,
            nullptr,
            true
        );

    switch (Result)
    {
    case EPathFollowingRequestResult::RequestSuccessful:

        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Movement started"));

        break;


    case EPathFollowingRequestResult::AlreadyAtGoal:

        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAI] Already at target"));

        break;


    case EPathFollowingRequestResult::Failed:

        UE_LOG(LogTemp, Error,
            TEXT("[EnemyAI] Movement FAILED"));

        break;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("===================================="));
}


// ОСТАНОВКА ПЕРЕМЕЩЕНИЯ

void AEnemyAI::StopMovement()
{
    AAIController* EnemyController =
        Cast<AAIController>(GetController());

    if (EnemyController)
    {
        EnemyController->StopMovement();
    }
}


// ОБНАРУЖЕНИЕ ПРЕГРАДЫ

void AEnemyAI::OnObstacleDetected(AActor* Obstacle)
{
    // Базовая реализация пустая.
    // Поведение реализуется в дочерних классах.
}


// СМЕРТЬ ВРАГА

void AEnemyAI::Die()
{
    if (!IsAlive())
    {
        return;
    }

    // Останавливаем таймер
    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    // Прекращаем движение
    StopMovement();

    // Вызываем базовую реализацию смерти
    Super::Die();
}


// ЗАВЕРШЕНИЕ РАБОТЫ

void AEnemyAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(
        MovementStartTimer
    );

    Super::EndPlay(EndPlayReason);
}
