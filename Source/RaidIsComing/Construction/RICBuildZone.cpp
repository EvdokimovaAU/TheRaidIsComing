#include "RICBuildZone.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

ARICBuildZone::ARICBuildZone()
{
    PrimaryActorTick.bCanEverTick = false;

    ZoneBounds = CreateDefaultSubobject<UBoxComponent>(
        TEXT("ZoneBounds")
    );

    SetRootComponent(ZoneBounds);

    ZoneBounds->SetBoxExtent(
        FVector(HalfSize.X, HalfSize.Y, 10.0)
    );

    ZoneBounds->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    ZoneBounds->SetGenerateOverlapEvents(false);
    ZoneBounds->SetCanEverAffectNavigation(false);
    ZoneBounds->SetHiddenInGame(true);

    HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("HighlightMesh")
    );

    HighlightMesh->SetupAttachment(ZoneBounds);

    HighlightMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    HighlightMesh->SetGenerateOverlapEvents(false);
    HighlightMesh->SetCanEverAffectNavigation(false);
    HighlightMesh->SetCastShadow(false);

    // Модель и материал назначим в Blueprint.
}

void ARICBuildZone::OnConstruction(
    const FTransform& Transform
)
{
    Super::OnConstruction(Transform);

    // Защищаем размеры от нуля и некорректных значений.
    if (!FMath::IsFinite(HalfSize.X) || HalfSize.X < 1.0)
    {
        HalfSize.X = 1.0;
    }

    if (!FMath::IsFinite(HalfSize.Y) || HalfSize.Y < 1.0)
    {
        HalfSize.Y = 1.0;
    }

    ZoneBounds->SetBoxExtent(
        FVector(HalfSize.X, HalfSize.Y, 10.0)
    );

    // Немного поднимаем подсветку над поверхностью пола.
    HighlightMesh->SetRelativeLocation(
        FVector(0.0, 0.0, 2.0)
    );

    // Расчёт для стандартного Engine/BasicShapes/Plane:
    // его исходный размер — 100 × 100 см.
    HighlightMesh->SetRelativeScale3D(
        FVector(
            HalfSize.X * 2.0 / 100.0,
            HalfSize.Y * 2.0 / 100.0,
            1.0
        )
    );
}

void ARICBuildZone::BeginPlay()
{
    // При запуске зона не подсвечена.
    // Делается до Blueprint-события BeginPlay.
    SetHighlighted(false);

    Super::BeginPlay();
}

void ARICBuildZone::SetHighlighted(bool bEnabled)
{
    HighlightMesh->SetVisibility(bEnabled);
}

bool ARICBuildZone::IsBuildingAllowed(
    ERICBuildingType Type
) const
{
    switch (Type)
    {
        case ERICBuildingType::Barracks:
            return bAllowBarracks;

        case ERICBuildingType::ArcherTower:
            return bAllowArcherTowers;

        case ERICBuildingType::Barrier:
            return false;

        default:
            return false;
    }
}