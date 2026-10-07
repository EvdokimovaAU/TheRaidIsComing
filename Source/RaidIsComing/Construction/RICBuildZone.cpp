#include "RICBuildZone.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

ARICBuildZone::ARICBuildZone()
{
    PrimaryActorTick.bCanEverTick = false;

    ZoneBounds = CreateDefaultSubobject<UBoxComponent>(
        TEXT("ZoneBounds"));

    SetRootComponent(ZoneBounds);

    ZoneBounds->SetBoxExtent(
        FVector(HalfSize.X, HalfSize.Y, 10.0));

    ZoneBounds->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    ZoneBounds->SetGenerateOverlapEvents(false);
    ZoneBounds->SetCanEverAffectNavigation(false);
    ZoneBounds->SetHiddenInGame(true);

    // Сохраняем существующий компонент прямоугольной зоны.
    HighlightMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("HighlightMesh"));

    HighlightMesh->SetupAttachment(ZoneBounds);

    HighlightMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    HighlightMesh->SetGenerateOverlapEvents(false);
    HighlightMesh->SetCanEverAffectNavigation(false);
    HighlightMesh->SetCastShadow(false);

    // Новый компонент кольцевой зоны.
    SectorHighlightMesh =
        CreateDefaultSubobject<UProceduralMeshComponent>(
            TEXT("SectorHighlightMesh"));

    SectorHighlightMesh->SetupAttachment(ZoneBounds);

    SectorHighlightMesh->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);

    SectorHighlightMesh->SetGenerateOverlapEvents(false);
    SectorHighlightMesh->SetCanEverAffectNavigation(false);
    SectorHighlightMesh->SetCastShadow(false);
    SectorHighlightMesh->SetVisibility(false);
}

void ARICBuildZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Защита параметров от некорректных значений.
    if (!FMath::IsFinite(HalfSize.X) || HalfSize.X < 1.0)
    {
        HalfSize.X = 1.0;
    }

    if (!FMath::IsFinite(HalfSize.Y) || HalfSize.Y < 1.0)
    {
        HalfSize.Y = 1.0;
    }

    if (!FMath::IsFinite(InnerRadius))
    {
        InnerRadius = 250.0f;
    }

    InnerRadius = FMath::Max(InnerRadius, 1.0f);

    if (!FMath::IsFinite(OuterRadius))
    {
        OuterRadius = InnerRadius + 350.0f;
    }

    OuterRadius = FMath::Max(
        OuterRadius,
        InnerRadius + 1.0f);

    if (!FMath::IsFinite(StartAngleDegrees))
    {
        StartAngleDegrees = 0.0f;
    }

    StartAngleDegrees = FMath::Fmod(StartAngleDegrees, 360.0f);

    if (!FMath::IsFinite(ArcAngleDegrees))
    {
        ArcAngleDegrees = 120.0f;
    }

    ArcAngleDegrees = FMath::Clamp(
        ArcAngleDegrees,
        1.0f,
        360.0f);

    SegmentsPerCircle = FMath::Clamp(
        SegmentsPerCircle,
        12,
        512);

    // Прямоугольник: стандартный Plane имеет размер 100 x 100 см.
    HighlightMesh->SetRelativeLocation(
        FVector(0.0, 0.0, 2.0));

    HighlightMesh->SetRelativeRotation(
        FRotator::ZeroRotator);

    HighlightMesh->SetRelativeScale3D(
        FVector(
            HalfSize.X * 2.0 / 100.0,
            HalfSize.Y * 2.0 / 100.0,
            1.0
        ));

    // Вершины сектора уже рассчитаны в сантиметрах.
    SectorHighlightMesh->SetRelativeLocation(
        FVector(0.0, 0.0, 2.0));

    SectorHighlightMesh->SetRelativeRotation(
        FRotator::ZeroRotator);

    SectorHighlightMesh->SetRelativeScale3D(
        FVector::OneVector);

    if (ZoneShape == ERICBuildZoneShape::Rectangle)
    {
        ZoneBounds->SetBoxExtent(
            FVector(HalfSize.X, HalfSize.Y, 10.0));

        SectorHighlightMesh->ClearAllMeshSections();
    }
    else
    {
        // Это только вспомогательная рамка вокруг кольца.
        ZoneBounds->SetBoxExtent(
            FVector(OuterRadius, OuterRadius, 10.0));

        RebuildSectorMesh();
    }

    if (HighlightMaterial)
    {
        HighlightMesh->SetMaterial(0, HighlightMaterial.Get());
    }

    // Для старого Blueprint можем взять материал с его Plane.
    UMaterialInterface* SectorMaterial = HighlightMaterial.Get();

    if (!SectorMaterial)
    {
        SectorMaterial = HighlightMesh->GetMaterial(0);
    }

    SectorHighlightMesh->SetMaterial(0, SectorMaterial);

    RefreshHighlightVisibility();
}

void ARICBuildZone::RebuildSectorMesh()
{
    SectorHighlightMesh->ClearAllMeshSections();

    const int32 SegmentCount = FMath::Max(
        1,
        FMath::CeilToInt(
            SegmentsPerCircle * ArcAngleDegrees / 360.0f)
    );

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve((SegmentCount + 1) * 2);
    UV0.Reserve((SegmentCount + 1) * 2);
    Triangles.Reserve(SegmentCount * 6);

    for (int32 Index = 0; Index <= SegmentCount; ++Index)
    {
        const float Alpha =
            static_cast<float>(Index) / SegmentCount;

        const float AngleDegrees =
            StartAngleDegrees + ArcAngleDegrees * Alpha;

        const float AngleRadians =
            FMath::DegreesToRadians(AngleDegrees);

        const FVector Direction(
            FMath::Cos(AngleRadians),
            FMath::Sin(AngleRadians),
            0.0
        );

        const FVector InnerPoint = Direction * InnerRadius;
        const FVector OuterPoint = Direction * OuterRadius;

        Vertices.Add(InnerPoint);
        Vertices.Add(OuterPoint);

        UV0.Add(FVector2D(
            0.5 + InnerPoint.X / (OuterRadius * 2.0),
            0.5 + InnerPoint.Y / (OuterRadius * 2.0)
        ));

        UV0.Add(FVector2D(
            0.5 + OuterPoint.X / (OuterRadius * 2.0),
            0.5 + OuterPoint.Y / (OuterRadius * 2.0)
        ));

        if (Index < SegmentCount)
        {
            const int32 InnerCurrent = Index * 2;
            const int32 OuterCurrent = InnerCurrent + 1;
            const int32 InnerNext = InnerCurrent + 2;
            const int32 OuterNext = InnerCurrent + 3;

            // Первый треугольник полосы.
            Triangles.Add(InnerCurrent);
            Triangles.Add(OuterCurrent);
            Triangles.Add(OuterNext);

            // Второй треугольник полосы.
            Triangles.Add(InnerCurrent);
            Triangles.Add(OuterNext);
            Triangles.Add(InnerNext);
        }
    }

    Normals.Init(FVector::UpVector, Vertices.Num());
    VertexColors.Init(FLinearColor::White, Vertices.Num());

    Tangents.Init(
        FProcMeshTangent(1.0f, 0.0f, 0.0f),
        Vertices.Num()
    );

    const TArray<FVector2D> EmptyUVs;

    SectorHighlightMesh->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        EmptyUVs,
        EmptyUVs,
        EmptyUVs,
        VertexColors,
        Tangents,
        false,
        false
    );
}

void ARICBuildZone::BeginPlay()
{
    SetHighlighted(false);
    Super::BeginPlay();
}

void ARICBuildZone::SetHighlighted(bool bEnabled)
{
    bIsHighlighted = bEnabled;
    RefreshHighlightVisibility();
}

void ARICBuildZone::RefreshHighlightVisibility()
{
    const bool bRectangle =
        ZoneShape == ERICBuildZoneShape::Rectangle;

    HighlightMesh->SetVisibility(
        bIsHighlighted && bRectangle);

    SectorHighlightMesh->SetVisibility(
        bIsHighlighted && !bRectangle);
}

bool ARICBuildZone::IsBuildingAllowed(ERICBuildingType Type) const
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