#include "RICConstructionComponent.h"

#include "RICBuilding.h"
#include "RICBuildZone.h"
#include "RICPlacementMath.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

namespace
{
    bool IsSupportedZone(const ARICBuildZone& Zone)
    {
        // Зона должна быть горизонтальной.
        // Поворот вокруг вертикальной оси разрешён.
        return Zone.GetActorScale3D().Equals(
            FVector::OneVector, 0.0001)
            && Zone.GetActorUpVector().Equals(
                FVector::UpVector, 0.0001);
    }

    bool FindPositionInZone(
        const ARICBuildZone& Zone,
        const FVector2D& FootprintHalfSize,
        const FVector& WantedCenter,
        FVector& OutCenter)
    {
        if (!IsSupportedZone(Zone))
        {
            return false;
        }

        const FTransform Transform = Zone.GetActorTransform();

        const FVector LocalWanted =
            Transform.InverseTransformPosition(WantedCenter);

        const FVector LocalX =
            Transform.InverseTransformVectorNoScale(
                FVector::ForwardVector);

        const FVector LocalY =
            Transform.InverseTransformVectorNoScale(
                FVector::RightVector);

        RICPlacement::FRegion Region;

        Region.bRing =
            Zone.ZoneShape == ERICBuildZoneShape::RingSector;

        Region.ZoneHalf = Zone.HalfSize;
        Region.BuildingHalf = FootprintHalfSize;

        Region.AxisX = FVector2D(LocalX.X, LocalX.Y);
        Region.AxisY = FVector2D(LocalY.X, LocalY.Y);

        Region.Inner = Zone.InnerRadius;
        Region.Outer = Zone.OuterRadius;
        Region.StartDegrees = Zone.StartAngleDegrees;
        Region.ArcDegrees = Zone.ArcAngleDegrees;

        FVector2D LocalResult;

        if (!RICPlacement::FindNearest(
            Region,
            FVector2D(LocalWanted.X, LocalWanted.Y),
            LocalResult))
        {
            return false;
        }

        OutCenter = Transform.TransformPosition(
            FVector(LocalResult.X, LocalResult.Y, 0.0));

        return true;
    }
}

URICConstructionComponent::URICConstructionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    bAutoActivate = false;
}

bool URICConstructionComponent::StartPreview(
    TSubclassOf<ARICBuilding> BuildingClass)
{
    APlayerController* Controller =
        Cast<APlayerController>(GetOwner());

    UWorld* World = GetWorld();

    if (!Controller
        || !Controller->IsLocalController()
        || !World
        || !BuildingClass)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cannot start construction preview: invalid controller or building class."));

        return false;
    }

    const ARICBuilding* Defaults =
        BuildingClass->GetDefaultObject<ARICBuilding>();

    const UStaticMeshComponent* SourceMesh =
        Defaults ? Defaults->BuildingMesh.Get() : nullptr;

    if (!SourceMesh || !SourceMesh->GetStaticMesh())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cannot start construction preview: BuildingMesh has no static mesh."));

        return false;
    }

    if (Defaults->BuildingType == ERICBuildingType::Barrier)
    {
        return false;
    }

    CancelPreview();

    FActorSpawnParameters SpawnParams;

    SpawnParams.Owner = Controller;
    SpawnParams.ObjectFlags |= RF_Transient;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    PreviewActor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams);

    if (!PreviewActor)
    {
        return false;
    }

    PreviewActor->SetActorHiddenInGame(true);
    PreviewActor->SetActorEnableCollision(false);
    PreviewActor->SetCanBeDamaged(false);

    UStaticMeshComponent* Mesh =
        PreviewActor->GetStaticMeshComponent();

    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->SetCastShadow(false);

    Mesh->SetStaticMesh(SourceMesh->GetStaticMesh());

    // Копируем размер меша из Blueprint казармы.
    PreviewActor->SetActorScale3D(
        SourceMesh->GetRelativeScale3D());

    for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Material = PreviewMaterial
            ? PreviewMaterial.Get()
            : SourceMesh->GetMaterial(Index);

        Mesh->SetMaterial(Index, Material);
    }

    const FBoxSphereBounds Bounds =
        Mesh->CalcBounds(Mesh->GetComponentTransform());

    FootprintHalfSize = FVector2D(
        Bounds.BoxExtent.X,
        Bounds.BoxExtent.Y);

    if (FootprintHalfSize.ContainsNaN()
        || FootprintHalfSize.X <= 0.0
        || FootprintHalfSize.Y <= 0.0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cannot start construction preview: invalid mesh dimensions."));

        CancelPreview();
        return false;
    }

    // Учитываем меши, у которых pivot находится не в центре.
    PreviewCenterOffset =
        Bounds.Origin - PreviewActor->GetActorLocation();

    PreviewHeightOffset =
        Bounds.BoxExtent.Z - PreviewCenterOffset.Z;

    SelectedType = Defaults->BuildingType;

    for (TActorIterator<ARICBuildZone> It(World); It; ++It)
    {
        if (It->IsBuildingAllowed(SelectedType)
            && !IsSupportedZone(**It))
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Build zone %s must have Scale 1,1,1 and no Pitch/Roll."),
                *It->GetName());
        }
    }

    UpdateZoneHighlights(true);

    SetComponentTickEnabled(true);
    UpdatePreviewPosition();

    if (!bHasValidZonePosition)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Preview has no valid zone position. Check zone size, transform and cursor position."));
    }

    return true;
}

void URICConstructionComponent::CancelPreview()
{
    SetComponentTickEnabled(false);

    bHasValidZonePosition = false;

    if (IsValid(PreviewActor.Get()))
    {
        PreviewActor->Destroy();
    }

    PreviewActor = nullptr;

    UpdateZoneHighlights(false);
}

bool URICConstructionComponent::IsBuildingMode() const
{
    return IsValid(PreviewActor.Get());
}

bool URICConstructionComponent::HasValidZonePosition() const
{
    return IsBuildingMode() && bHasValidZonePosition;
}

void URICConstructionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(
        DeltaTime,
        TickType,
        ThisTickFunction);

    if (!IsBuildingMode())
    {
        CancelPreview();
        return;
    }

    UpdatePreviewPosition();
}

void URICConstructionComponent::UpdatePreviewPosition()
{
    bHasValidZonePosition = false;

    if (!IsBuildingMode())
    {
        return;
    }

    APlayerController* Controller =
        Cast<APlayerController>(GetOwner());

    UWorld* World = GetWorld();

    if (!Controller || !World)
    {
        PreviewActor->SetActorHiddenInGame(true);
        return;
    }

    FVector RayOrigin;
    FVector RayDirection;

    if (!Controller->DeprojectMousePositionToWorld(
        RayOrigin,
        RayDirection)
        || FMath::Abs(RayDirection.Z) < 0.000001)
    {
        PreviewActor->SetActorHiddenInGame(true);
        return;
    }

    double BestDistanceSquared = 1.e30;
    FVector BestLocation = FVector::ZeroVector;

    for (TActorIterator<ARICBuildZone> It(World); It; ++It)
    {
        ARICBuildZone* Zone = *It;

        if (!Zone->IsBuildingAllowed(SelectedType)
            || !IsSupportedZone(*Zone))
        {
            continue;
        }

        // Пересечение луча курсора с плоскостью этой зоны.
        const double ZoneZ = Zone->GetActorLocation().Z;

        const double DistanceAlongRay =
            (ZoneZ - RayOrigin.Z) / RayDirection.Z;

        if (DistanceAlongRay < 0.0)
        {
            continue;
        }

        const FVector CursorOnPlane =
            RayOrigin + RayDirection * DistanceAlongRay;

        FVector WantedCenter = CursorOnPlane;

        WantedCenter.X += PreviewCenterOffset.X;
        WantedCenter.Y += PreviewCenterOffset.Y;

        FVector ValidCenter;

        if (!FindPositionInZone(
            *Zone,
            FootprintHalfSize,
            WantedCenter,
            ValidCenter))
        {
            continue;
        }

        const FVector CandidateLocation(
            ValidCenter.X - PreviewCenterOffset.X,
            ValidCenter.Y - PreviewCenterOffset.Y,
            ZoneZ + PreviewHeightOffset);

        const double DX =
            CandidateLocation.X - CursorOnPlane.X;

        const double DY =
            CandidateLocation.Y - CursorOnPlane.Y;

        const double DistanceSquared = DX * DX + DY * DY;

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestLocation = CandidateLocation;

            bHasValidZonePosition = true;
        }
    }

    PreviewActor->SetActorHiddenInGame(
        !bHasValidZonePosition);

    if (bHasValidZonePosition)
    {
        PreviewActor->SetActorLocation(BestLocation);
    }
}

void URICConstructionComponent::UpdateZoneHighlights(
    bool bEnabled)
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    for (TActorIterator<ARICBuildZone> It(World); It; ++It)
    {
        ARICBuildZone* Zone = *It;

        bool bShow = false;

        if (bEnabled && Zone->IsBuildingAllowed(SelectedType))
        {
            FVector AnyValidCenter;

            // Подсвечиваем только зоны, куда постройка помещается.
            bShow = FindPositionInZone(
                *Zone,
                FootprintHalfSize,
                Zone->GetActorLocation(),
                AnyValidCenter);
        }

        Zone->SetHighlighted(bShow);
    }
}

void URICConstructionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    CancelPreview();

    Super::EndPlay(EndPlayReason);
}