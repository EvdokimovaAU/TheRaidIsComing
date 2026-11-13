
#include "RICBuilding.h"

#include "AI/GameBalanceManager.h"
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

    SetCanBeDamaged(false);
}

bool ARICBuilding::IsDestructible() const
{
    return BuildingType == ERICBuildingType::Barrier ||
        BuildingType == ERICBuildingType::Capitol;
}

int32 ARICBuilding::GetConstructionCost(const UObject* WorldContext) const
{
    if (const AGameBalanceManager* Balance = AGameBalanceManager::GetBalanceManager(WorldContext))
    {
        switch (BuildingType)
        {
        case ERICBuildingType::ArcherTower:
            return FMath::Max(0, Balance->ArcherTower.BuildCost);
        case ERICBuildingType::MageTower:
            return FMath::Max(0, Balance->MageTower.BuildCost);
        default:
            break;
        }
    }

    return ConstructionCost;
}

bool ARICBuilding::IsAlive() const
{
    return IsDestructible() &&
        CanBeDamaged() &&
        !IsActorBeingDestroyed() &&
        CurrentHP > 0.0f;
}

void ARICBuilding::BeginPlay()
{
    Super::BeginPlay();

    BuildingId = FGuid::NewGuid();

    const bool bDestructible = IsDestructible();

    SetCanBeDamaged(bDestructible);

    // Only Capitol receives global health settings.
    if (BuildingType == ERICBuildingType::Capitol)
    {
        if (AGameBalanceManager* Balance =
            AGameBalanceManager::GetBalanceManager(this))
        {
            MaxHP = FMath::Max(
                1.0f,
                Balance->General.CapitolMaxHealth
            );
        }
    }

    if (bDestructible)
    {
        if (!FMath::IsFinite(MaxHP) || MaxHP < 1.0f)
        {
            MaxHP = 100.0f;
        }

        CurrentHP = MaxHP;
    }
    else
    {
        CurrentHP = 0.0f;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("[Building] Initialized: %s | Type: %d | Destructible: %s | HP: %.1f"),
        *GetName(),
        static_cast<int32>(BuildingType),
        bDestructible ? TEXT("Yes") : TEXT("No"),
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
    if (!IsAlive() ||
        !FMath::IsFinite(DamageAmount) ||
        DamageAmount <= 0.0f)
    {
        return 0.0f;
    }

    const float AppliedDamage =
        FMath::Min(DamageAmount, CurrentHP);

    CurrentHP = FMath::Max(
        0.0f,
        CurrentHP - AppliedDamage
    );

    Super::TakeDamage(
        AppliedDamage,
        DamageEvent,
        EventInstigator,
        DamageCauser
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("[Building] %s received %.1f damage | HP: %.1f / %.1f"),
        *GetName(),
        AppliedDamage,
        CurrentHP,
        MaxHP
    );

    if (CurrentHP <= 0.0f)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[Building] DESTROYED: %s"),
            *GetName()
        );

        Destroy();
    }

    return AppliedDamage;
}
