
#include "RICBuilding.h"
#include "UI/RICOverheadHealthBar.h"

#include "AI/GameBalanceManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavAreas/NavArea_Null.h"

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

void ARICBuilding::PreRegisterAllComponents()
{
    if (BuildingType == ERICBuildingType::ArcherTower ||
        BuildingType == ERICBuildingType::MageTower ||
        BuildingType == ERICBuildingType::Barrier)
    {
        BuildingMesh->SetCanEverAffectNavigation(false);
    }

    Super::PreRegisterAllComponents();
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

        if (BuildingType == ERICBuildingType::Barrier)
        {
            const FString Level = UGameplayStatics::GetCurrentLevelName(this);
            if (Level == TEXT("Level_2") || Level == TEXT("Level_3"))
            {
                MaxHP *= 2.f;
            }
        }
        CurrentHP = MaxHP;
    }
    else
    {
        CurrentHP = 0.0f;
    }

    if (BuildingType == ERICBuildingType::Barrier && BuildingMesh->GetStaticMesh() &&
        BuildingMesh->IsCollisionEnabled())
    {
        // An area modifier updates walkability without exporting the fence mesh
        // as new geometry on every placement and removal.
        const FBoxSphereBounds MeshBounds = BuildingMesh->CalcBounds(FTransform::Identity);
        auto* NavigationObstacle = NewObject<UBoxComponent>(this, TEXT("BarrierNavigationObstacle"));
        NavigationObstacle->SetupAttachment(BuildingMesh);
        NavigationObstacle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NavigationObstacle->SetGenerateOverlapEvents(false);
        NavigationObstacle->SetBoxExtent(MeshBounds.BoxExtent + FVector(0, 0, 30));
        NavigationObstacle->SetRelativeLocation(MeshBounds.Origin);
        NavigationObstacle->bDynamicObstacle = true;
        NavigationObstacle->SetAreaClassOverride(UNavArea_Null::StaticClass());
        NavigationObstacle->SetCanEverAffectNavigation(true);
        NavigationObstacle->RegisterComponent();
    }

    if (BuildingType == ERICBuildingType::Capitol)
    {
        FVector Center, Extent;
        GetActorBounds(false, Center, Extent);
        const float RoofClearance = FMath::Max(25.f, 0.5f * Extent.GetAbsMax());
        URICOverheadHealthBar::Attach(this, Center + FVector::UpVector * (Extent.Z + RoofClearance),
            FIntPoint(168, 16), TAttribute<FVector2D>::CreateWeakLambda(this, [this]()
            {
                return FVector2D(CurrentHP, MaxHP);
            }), true);
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
