
#include "AI/MageProjectile.h"

#include "AI/AIBase.h"
#include "AI/EnemyAI.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// ==========================================
// Конструктор
// ==========================================

AMageProjectile::AMageProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    // Создаём корневой компонент
    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );

    RootComponent = SceneRoot;

    // Попадание определяем программно
    SetActorEnableCollision(false);

    // Защита от бесконечного существования снаряда
    InitialLifeSpan = 10.0f;
}

// ==========================================
// Начало игры
// ==========================================

void AMageProjectile::BeginPlay()
{
    Super::BeginPlay();
}

// ==========================================
// Инициализация магического снаряда
// ==========================================

void AMageProjectile::InitializeProjectile(
    AAIBase* NewTarget,
    float NewDamage,
    float NewSplashRadius)
{
    // Проверяем, существует ли противник
    if (!IsValid(NewTarget) || !NewTarget->IsAlive())
    {
        Destroy();
        return;
    }

    TargetActor = NewTarget;

    // Получаем урон от башни
    ProjectileDamage = FMath::Max(
        0.0f,
        NewDamage
    );

    // Получаем радиус взрыва от башни
    ProjectileSplashRadius = FMath::Max(
        0.0f,
        NewSplashRadius
    );

    bHasExploded = false;
    bInitialized = true;
}

// ==========================================
// Движение магического шара
// ==========================================

void AMageProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Снаряд ещё не получил данные от башни
    // либо уже взорвался
    if (!bInitialized || bHasExploded)
    {
        return;
    }

    // Если цель уничтожена до попадания,
    // удаляем снаряд
    if (!TargetActor.IsValid() || !TargetActor->IsAlive())
    {
        Destroy();
        return;
    }

    const FVector CurrentLocation =
        GetActorLocation();

    // Получаем актуальную позицию движущейся цели
    const FVector TargetLocation =
        TargetActor->GetActorLocation();

    // Вектор до противника
    const FVector ToTarget =
        TargetLocation - CurrentLocation;

    // Расстояние до противника
    const float Distance =
        ToTarget.Size();

    // Если шар уже достиг противника
    if (Distance <= HitRadius)
    {
        HitTarget();
        return;
    }

    // Направление движения
    const FVector Direction =
        ToTarget.GetSafeNormal();

    // Расстояние, которое шар проходит за кадр
    const float Step =
        FMath::Max(FlightSpeed, 1.0f) * DeltaTime;

    // Если за этот кадр шар должен достичь цели
    if (Step >= Distance)
    {
        SetActorLocation(TargetLocation);

        HitTarget();
        return;
    }

    // Перемещаем магический шар
    const FVector NewLocation =
        CurrentLocation + Direction * Step;

    SetActorLocation(NewLocation);

    // Поворачиваем снаряд по направлению движения
    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }
}

// ==========================================
// Попадание магического шара
// ==========================================

void AMageProjectile::HitTarget()
{
    // Защита от повторного взрыва
    if (!bInitialized || bHasExploded)
    {
        return;
    }

    bHasExploded = true;
    bInitialized = false;

    // Запоминаем точку попадания
    const FVector ImpactLocation =
        GetActorLocation();

    // Наносим массовый урон
    ApplySplashDamage(ImpactLocation);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[MageProjectile] Explosion | Damage: %.1f | Radius: %.1f"),
        ProjectileDamage,
        ProjectileSplashRadius
    );

    // Удаляем снаряд после взрыва
    Destroy();
}

// ==========================================
// Массовый урон
// ==========================================

void AMageProjectile::ApplySplashDamage(
    const FVector& ImpactLocation)
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    int32 HitEnemiesCount = 0;

    // Перебираем всех вражеских AI на уровне
    for (TActorIterator<AEnemyAI> It(World); It; ++It)
    {
        AEnemyAI* Enemy = *It;

        // Пропускаем несуществующих и мёртвых врагов
        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        // Расстояние от центра взрыва до противника
        const float Distance = FVector::Dist(
            ImpactLocation,
            Enemy->GetActorLocation()
        );

        // Проверяем попадание в радиус взрыва
        if (Distance > ProjectileSplashRadius)
        {
            continue;
        }

        // Наносим урон конкретному противнику
        Enemy->ReceiveDamage(ProjectileDamage);

        ++HitEnemiesCount;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[MageProjectile] Splash hit: %s | Damage: %.1f | Distance: %.1f"),
            *Enemy->GetName(),
            ProjectileDamage,
            Distance
        );
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[MageProjectile] Total enemies hit: %d"),
        HitEnemiesCount
    );
}
