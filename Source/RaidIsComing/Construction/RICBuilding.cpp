#include "RICBuilding.h"

#include "Components/StaticMeshComponent.h"

ARICBuilding::ARICBuilding()
{
    PrimaryActorTick.bCanEverTick = false;

    BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BuildingMesh")
    );

    SetRootComponent(BuildingMesh);

    BuildingMesh->SetCollisionProfileName(TEXT("BlockAll"));
    BuildingMesh->SetSimulatePhysics(false);

    SetCanBeDamaged(true);
}

void ARICBuilding::BeginPlay()
{
    Super::BeginPlay();

    // Каждый экземпляр получает собственный ID при запуске игры.
    BuildingId = FGuid::NewGuid();

    // Защита от некорректной настройки здоровья.
    if (!FMath::IsFinite(MaxHP) || MaxHP < 1.0f)
    {
        MaxHP = 1.0f;
    }

    CurrentHP = MaxHP;

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Building spawned: %s | ID: %s | HP: %.1f"),
        *GetName(),
        *BuildingId.ToString(),
        CurrentHP
    );
}

float ARICBuilding::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    if (!CanBeDamaged()
        || IsActorBeingDestroyed()
        || CurrentHP <= 0.0f
        || !FMath::IsFinite(DamageAmount)
        || DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    const float AppliedDamage = FMath::Min(
        DamageAmount,
        CurrentHP
    );

    CurrentHP = FMath::Max(
        0.0f,
        CurrentHP - AppliedDamage
    );

    // Сохраняем стандартные события получения урона Unreal.
    Super::TakeDamage(
        AppliedDamage,
        DamageEvent,
        EventInstigator,
        DamageCauser
    );

    if (CurrentHP <= 0.0f)
    {
        Destroy();
    }

    return AppliedDamage;
}