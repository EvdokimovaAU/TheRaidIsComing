
#include "AI/ArcherProjectile.h"
#include "AI/AIBase.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"

AArcherProjectile::AArcherProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("SceneRoot")
    );

    RootComponent = SceneRoot;

    // Физические столкновения не используем.
    // Попадание определяем программно.
    SetActorEnableCollision(false);

    InitialLifeSpan = 10.0f;
}

void AArcherProjectile::BeginPlay()
{
    Super::BeginPlay();

    StartLocation = GetActorLocation();
}

void AArcherProjectile::InitializeProjectile(
    AAIBase* NewTarget,
    float NewDamage)
{
    if (!IsValid(NewTarget) || !NewTarget->IsAlive())
    {
        Destroy();
        return;
    }

    TargetActor = NewTarget;

    ProjectileDamage = FMath::Max(0.0f, NewDamage);

    StartLocation = GetActorLocation();

    // Продолжительность полёта зависит от расстояния.
    const float Distance = FVector::Dist(
        StartLocation,
        NewTarget->GetActorLocation()
    );

    FlightDuration = FMath::Max(
        Distance / FMath::Max(FlightSpeed, 1.0f),
        0.05f
    );

    ElapsedTime = 0.0f;
    bInitialized = true;
}

void AArcherProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bInitialized)
    {
        return;
    }

    // Цель уничтожена или уже мертва.
    if (!TargetActor.IsValid() || !TargetActor->IsAlive())
    {
        Destroy();
        return;
    }

    ElapsedTime += DeltaTime;

    // Прогресс полёта: от 0 до 1.
    const float Alpha = FMath::Clamp(
        ElapsedTime / FlightDuration,
        0.0f,
        1.0f
    );

    // Каждый кадр обновляем конечную точку,
    // чтобы учитывать перемещение противника.
    const FVector EndLocation =
        TargetActor->GetActorLocation();

    // Линейное движение от старта к цели.
    FVector NewLocation = FMath::Lerp(
        StartLocation,
        EndLocation,
        Alpha
    );

    // Добавляем вертикальную дугу.
    // В начале и конце высота равна нулю.
    const float Height =
        4.0f * ArcHeight * Alpha * (1.0f - Alpha);

    NewLocation.Z += Height;

    // Поворачиваем стрелу по направлению движения.
    const FVector Direction =
        NewLocation - GetActorLocation();

    if (!Direction.IsNearlyZero())
    {
        SetActorRotation(Direction.Rotation());
    }

    SetActorLocation(NewLocation);

    // Проверяем попадание.
    const float DistanceToTarget = FVector::Dist(
        NewLocation,
        EndLocation
    );

    if (DistanceToTarget <= HitRadius || Alpha >= 1.0f)
    {
        HitTarget();
    }
}

void AArcherProjectile::HitTarget()
{
    if (TargetActor.IsValid() && TargetActor->IsAlive())
    {
        TargetActor->ReceiveDamage(ProjectileDamage);

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Arrow] Hit %s | Damage: %.1f"),
            *TargetActor->GetName(),
            ProjectileDamage
        );
    }

    Destroy();
}