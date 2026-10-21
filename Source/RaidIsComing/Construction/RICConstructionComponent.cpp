#include "RICConstructionComponent.h"

#include "RICBuilding.h"
#include "RICBuildZone.h"
#include "RICPlacementMath.h"
#include "RICMoneyComponent.h"
#include "RICPassage.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

namespace
{
    // Проекция локального bounding box меша на плоскость XY.
    // Полуоси сохраняют поворот и масштаб вместо расширения до мирового AABB.
    struct FBuildingFootprint
    {
        FVector2D Center;
        FVector2D HalfAxes[3];
    };

    bool MakeBuildingFootprint(
        const UStaticMeshComponent* Mesh,
        const FTransform& Transform,
        FBuildingFootprint& Out)
    {
        if (!IsValid(Mesh) || !Mesh->GetStaticMesh()
            || Transform.ContainsNaN())
        {
            return false;
        }

        const FBoxSphereBounds Local = Mesh->CalcBounds(FTransform::Identity);
        if (Local.Origin.ContainsNaN() || Local.BoxExtent.ContainsNaN()
            || Local.BoxExtent.X <= 0.0 || Local.BoxExtent.Y <= 0.0)
        {
            return false;
        }

        const FVector Center = Transform.TransformPosition(Local.Origin);
        Out.Center = FVector2D(Center.X, Center.Y);
        const FVector LocalHalfAxes[3] = {
            FVector(Local.BoxExtent.X, 0.0, 0.0),
            FVector(0.0, Local.BoxExtent.Y, 0.0),
            FVector(0.0, 0.0, Local.BoxExtent.Z)
        };
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FVector Axis = Transform.TransformVector(LocalHalfAxes[Index]);
            Out.HalfAxes[Index] = FVector2D(Axis.X, Axis.Y);
        }
        return true;
    }

    bool FootprintsOverlap(
        const FBuildingFootprint& A,
        const FBuildingFootprint& B)
    {
        constexpr double ContactTolerance = 0.001;
        const FVector2D Delta = B.Center - A.Center;

        // SAT: достаточно найти одну ось, на которой площади разделены.
        // Для горизонтальных мешей это обычная проверка двух OBB.
        // Проекция третьей полуоси учитывает также наклон меша.
        for (int32 Shape = 0; Shape < 2; ++Shape)
        {
            const FBuildingFootprint& Source = Shape == 0 ? A : B;
            for (int32 Edge = 0; Edge < 3; ++Edge)
            {
                const FVector2D HalfAxis = Source.HalfAxes[Edge];
                const double Length = HalfAxis.Size();
                if (Length <= 1.e-8)
                {
                    continue;
                }
                const FVector2D Axis(-HalfAxis.Y / Length, HalfAxis.X / Length);
                double RadiusA = 0.0;
                double RadiusB = 0.0;
                for (int32 Index = 0; Index < 3; ++Index)
                {
                    RadiusA += FMath::Abs(FVector2D::DotProduct(A.HalfAxes[Index], Axis));
                    RadiusB += FMath::Abs(FVector2D::DotProduct(B.HalfAxes[Index], Axis));
                }
                const double Distance = FMath::Abs(FVector2D::DotProduct(Delta, Axis));
                if (Distance >= RadiusA + RadiusB - ContactTolerance)
                {
                    // Касание разрешено, как и в прежней проверке.
                    return false;
                }
            }
        }
        return true;
    }

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
    SelectedBuildingClass = BuildingClass;

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

    if (!bHasValidZonePosition && SelectedType != ERICBuildingType::Barrier)
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
    SelectedBuildingClass = nullptr;

    UpdateZoneHighlights(false);
    SelectedPassage.Reset();
    HidePassageHighlights();
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
    if (SelectedType == ERICBuildingType::Barrier)
    {
        UpdateBarrierPreviewPosition();
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
bool URICConstructionComponent::HasBuildingOverlap(
    const UStaticMeshComponent* CandidateMesh,
    const FTransform& CandidateTransform,
    const ARICBuilding* IgnoredBuilding) const
{
    UWorld* World = GetWorld();
    FBuildingFootprint Candidate;
    if (!World || !MakeBuildingFootprint(CandidateMesh, CandidateTransform, Candidate))
    {
        return true;
    }

    for (TActorIterator<ARICBuilding> It(World); It; ++It)
    {
        ARICBuilding* Building = *It;
        if (!IsValid(Building) || Building == IgnoredBuilding
            || Building->IsActorBeingDestroyed())
        {
            continue;
        }
        const UStaticMeshComponent* OtherMesh = Building->BuildingMesh.Get();
        if (!IsValid(OtherMesh) || !OtherMesh->GetStaticMesh())
        {
            continue;
        }
        FBuildingFootprint Other;
        if (!MakeBuildingFootprint(OtherMesh, OtherMesh->GetComponentTransform(), Other)
            || FootprintsOverlap(Candidate, Other))
        {
            return true;
        }
    }
    return false;
}

bool URICConstructionComponent::TryPlacePreview()
{
    if (!IsBuildingMode() || !SelectedBuildingClass)
    {
        return false;
    }
    if (SelectedType == ERICBuildingType::Barrier)
    {
        // Заново проверяем курсор и доступность прохода в момент клика.
        UpdateBarrierPreviewPosition();

        ARICPassage* Passage = SelectedPassage.Get();
        UWorld* World = GetWorld();
        const ARICBuilding* Defaults = SelectedBuildingClass.GetDefaultObject();
        URICMoneyComponent* Money = GetOwner()
            ? GetOwner()->FindComponentByClass<URICMoneyComponent>()
            : nullptr;

        if (!bHasValidZonePosition || !IsValid(Passage) || !World
            || !Defaults || !IsValid(Money)
            || Defaults->BuildingType != ERICBuildingType::Barrier)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: select an available passage and check the barrier class/money component."));
            return false;
        }

        FTransform PlacementTransform;
        if (!GetBarrierPreviewTransform(*Passage, PlacementTransform))
        {
            return false;
        }

        const int32 Cost = Defaults->ConstructionCost;
        if (!Money->CanAfford(Cost))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: cost %d, balance %d."),
                Cost, Money->CurrentMoney);
            return false;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = GetOwner();
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        ARICBuilding* Barrier = World->SpawnActor<ARICBuilding>(
            SelectedBuildingClass.Get(),
            PlacementTransform.GetLocation(),
            PlacementTransform.Rotator(),
            SpawnParams);

        if (!IsValid(Barrier) || Barrier->IsActorBeingDestroyed())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: could not spawn barrier."));
            return false;
        }

        // Как у башен: итоговый масштаб должен совпадать с превью.
        Barrier->SetActorTransform(PlacementTransform);

        UStaticMeshComponent* Mesh = Barrier->BuildingMesh.Get();
        if (!IsValid(Passage) || Passage->IsActorBeingDestroyed()
            || !Passage->IsOpen() || !IsValid(Money)
            || Barrier->BuildingType != ERICBuildingType::Barrier
            || !Mesh || !Mesh->GetStaticMesh())
        {
            Barrier->Destroy();
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: passage or spawned barrier is no longer valid."));
            return false;
        }

        // Blueprint мог изменить меш при создании.
        // Проверяем реальные размеры в координатах прохода:
        // так поворот прохода вокруг Z не мешает проверке границ.
        const FTransform MeshInPassage =
            Mesh->GetComponentTransform().GetRelativeTransform(
                Passage->GetActorTransform());
        const FBoxSphereBounds LocalBounds = Mesh->CalcBounds(MeshInPassage);
        const FBoxSphereBounds WorldBounds =
            Mesh->CalcBounds(Mesh->GetComponentTransform());

        constexpr double Tolerance = 0.01;
        const bool bFitsPassage =
            !LocalBounds.Origin.ContainsNaN()
            && !LocalBounds.BoxExtent.ContainsNaN()
            && !WorldBounds.Origin.ContainsNaN()
            && !WorldBounds.BoxExtent.ContainsNaN()
            && LocalBounds.BoxExtent.X > 0.0
            && LocalBounds.BoxExtent.Y > 0.0
            && FMath::Abs(LocalBounds.Origin.X) + LocalBounds.BoxExtent.X
            <= Passage->HalfSize.X + Tolerance
            && FMath::Abs(LocalBounds.Origin.Y) + LocalBounds.BoxExtent.Y
            <= Passage->HalfSize.Y + Tolerance
            && FMath::Abs(LocalBounds.Origin.Z - LocalBounds.BoxExtent.Z)
            <= Tolerance;

        if (!bFitsPassage || HasBuildingOverlap(Mesh, Mesh->GetComponentTransform(), Barrier))
        {
            Barrier->Destroy();
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: spawned mesh does not fit or overlaps another building."));
            return false;
        }

        // Сначала закрепляем преграду за проходом, затем оплачиваем.
        // Если оплата не прошла, уничтожение преграды снова откроет проход.
        if (!Passage->RegisterBarrier(Barrier))
        {
            Barrier->Destroy();
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: passage registration failed."));
            return false;
        }

        if (!Money->TrySpend(Cost))
        {
            Barrier->Destroy();
            UE_LOG(LogTemp, Warning,
                TEXT("Barrier rejected: payment failed."));
            return false;
        }

        UE_LOG(LogTemp, Log,
            TEXT("Barrier placed: %s. Passage: %s. Cost: %d. Balance: %d."),
            *Barrier->GetName(), *Passage->PassageId.ToString(),
            Cost, Money->CurrentMoney);

        CancelPreview();
        return true;
    }

    UpdatePreviewPosition();

    if (!bHasValidZonePosition)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: no valid zone position."));

        return false;
    }
    const ARICBuilding* BuildingDefaults =
        SelectedBuildingClass.GetDefaultObject();

    URICMoneyComponent* Money = GetOwner()
        ? GetOwner()->FindComponentByClass<URICMoneyComponent>()
        : nullptr;

    if (!BuildingDefaults || !Money)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: missing building defaults or money component.")
        );

        return false;
    }

    const int32 Cost = BuildingDefaults->ConstructionCost;

    if (!Money->CanAfford(Cost))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: cost %d, balance %d."),
            Cost,
            Money->CurrentMoney
        );

        return false;
    }

    UWorld* World = GetWorld();

    UStaticMeshComponent* PreviewMesh =
        PreviewActor->GetStaticMeshComponent();

    if (!World || !PreviewMesh || !PreviewMesh->GetStaticMesh())
    {
        return false;
    }

    if (HasBuildingOverlap(PreviewMesh, PreviewMesh->GetComponentTransform()))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: overlaps another building."));

        return false;
    }

    const FTransform PlacementTransform =
        PreviewActor->GetActorTransform();

    FActorSpawnParameters SpawnParams;

    SpawnParams.Owner = GetOwner();

    // Позицию проверяет наша логика строительства.
    // Unreal не должен автоматически сдвигать здание.
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ARICBuilding* NewBuilding = World->SpawnActor<ARICBuilding>(
        SelectedBuildingClass.Get(),
        PlacementTransform.GetLocation(),
        PlacementTransform.Rotator(),
        SpawnParams);

    if (!IsValid(NewBuilding)
        || NewBuilding->IsActorBeingDestroyed())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: could not spawn building."));

        return false;
    }

    // Устанавливаем тот же масштаб и позицию, что были у превью.
    NewBuilding->SetActorTransform(PlacementTransform);

    UStaticMeshComponent* BuildingMesh =
        NewBuilding->BuildingMesh.Get();

    if (!BuildingMesh || !BuildingMesh->GetStaticMesh())
    {
        NewBuilding->Destroy();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: spawned building has no mesh."));

        return false;
    }

    // Повторно проверяем реальный объект:
    // Blueprint мог изменить его размеры при создании.
    const FBoxSphereBounds ActualBounds =
        BuildingMesh->CalcBounds(
            BuildingMesh->GetComponentTransform());

    if (ActualBounds.Origin.ContainsNaN()
        || ActualBounds.BoxExtent.ContainsNaN()
        || ActualBounds.BoxExtent.X <= 0.0
        || ActualBounds.BoxExtent.Y <= 0.0)
    {
        NewBuilding->Destroy();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: invalid building dimensions."));

        return false;
    }

    const FVector2D ActualHalfSize(
        ActualBounds.BoxExtent.X,
        ActualBounds.BoxExtent.Y);

    bool bInsideAllowedZone = false;

    for (TActorIterator<ARICBuildZone> It(World); It; ++It)
    {
        ARICBuildZone* Zone = *It;

        if (!Zone->IsBuildingAllowed(NewBuilding->BuildingType))
        {
            continue;
        }

        const double BuildingBottom =
            ActualBounds.Origin.Z - ActualBounds.BoxExtent.Z;

        if (FMath::Abs(
            BuildingBottom - Zone->GetActorLocation().Z) > 0.01)
        {
            continue;
        }

        FVector NearestCenter;

        if (!FindPositionInZone(
            *Zone,
            ActualHalfSize,
            ActualBounds.Origin,
            NearestCenter))
        {
            continue;
        }

        const double DX =
            NearestCenter.X - ActualBounds.Origin.X;

        const double DY =
            NearestCenter.Y - ActualBounds.Origin.Y;

        if (DX * DX + DY * DY <= 0.0001)
        {
            bInsideAllowedZone = true;
            break;
        }
    }

    if (!bInsideAllowedZone
        || HasBuildingOverlap(BuildingMesh, BuildingMesh->GetComponentTransform(), NewBuilding))
    {
        NewBuilding->Destroy();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: spawned building does not fit."));

        return false;
    }
    if (!Money->TrySpend(Cost))
    {
        NewBuilding->Destroy();

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Construction rejected: payment failed.")
        );

        return false;
    }
    UE_LOG(
        LogTemp,
        Log,
        TEXT("Building placed: %s"),
        *NewBuilding->GetName());

    CancelPreview();

    return true;
}
bool URICConstructionComponent::GetBarrierPreviewTransform(
    const ARICPassage& Passage,
    FTransform& OutTransform) const
{
    if (!IsValid(PreviewActor.Get())
        || Passage.IsActorBeingDestroyed()
        || !Passage.IsOpen())
    {
        return false;
    }

    // Размер прохода задаём через HalfSize.
    // Разрешён только поворот вокруг вертикальной оси.
    if (!Passage.GetActorScale3D().Equals(
        FVector::OneVector, 0.0001)
        || !Passage.GetActorUpVector().Equals(
            FVector::UpVector, 0.0001))
    {
        return false;
    }

    constexpr double Tolerance = 0.001;

    if (FootprintHalfSize.X > Passage.HalfSize.X + Tolerance
        || FootprintHalfSize.Y > Passage.HalfSize.Y + Tolerance)
    {
        return false;
    }

    const FRotator Rotation(
        0.0,
        Passage.GetActorRotation().Yaw,
        0.0);

    // Смещение центра меша тоже поворачиваем вместе с преградой.
    const FVector RotatedOffset = Rotation.RotateVector(
        FVector(PreviewCenterOffset.X, PreviewCenterOffset.Y, 0.0));

    FVector Location = Passage.GetActorLocation() - RotatedOffset;
    Location.Z = Passage.GetActorLocation().Z + PreviewHeightOffset;

    OutTransform = FTransform(
        Rotation,
        Location,
        PreviewActor->GetActorScale3D());

    const UStaticMeshComponent* Mesh =
        PreviewActor->GetStaticMeshComponent();

    if (!Mesh || !Mesh->GetStaticMesh())
    {
        return false;
    }

    return !HasBuildingOverlap(Mesh, OutTransform);
}
void URICConstructionComponent::UpdateBarrierPreviewPosition()
{
    bHasValidZonePosition = false;
    SelectedPassage.Reset();

    UWorld* World = GetWorld();
    APlayerController* Controller =
        Cast<APlayerController>(GetOwner());

    if (!World || !Controller || !IsValid(PreviewActor.Get()))
    {
        HidePassageHighlights();

        if (IsValid(PreviewActor.Get()))
        {
            PreviewActor->SetActorHiddenInGame(true);
        }

        return;
    }

    FVector RayOrigin = FVector::ZeroVector;
    FVector RayDirection = FVector::ZeroVector;

    const bool bHasMouseRay =
        Controller->DeprojectMousePositionToWorld(
            RayOrigin, RayDirection)
        && FMath::Abs(RayDirection.Z) > 0.000001;

    double BestDistanceSquared = 1.e30;
    FTransform BestTransform = FTransform::Identity;

    for (TActorIterator<ARICPassage> It(World); It; ++It)
    {
        ARICPassage* Passage = *It;

        FTransform CandidateTransform;
        const bool bAvailable =
            GetBarrierPreviewTransform(*Passage, CandidateTransform);

        Passage->SetHighlighted(bAvailable);

        if (!bAvailable || !bHasMouseRay)
        {
            continue;
        }

        const double DistanceAlongRay =
            (Passage->GetActorLocation().Z - RayOrigin.Z)
            / RayDirection.Z;

        if (DistanceAlongRay < 0.0)
        {
            continue;
        }

        const FVector CursorOnPlane =
            RayOrigin + RayDirection * DistanceAlongRay;

        const FVector LocalCursor =
            Passage->GetActorTransform().InverseTransformPosition(
                CursorOnPlane);

        // Выбираем проход, только когда курсор внутри его площади.
        if (FMath::Abs(LocalCursor.X) > Passage->HalfSize.X
            || FMath::Abs(LocalCursor.Y) > Passage->HalfSize.Y)
        {
            continue;
        }

        const double DistanceSquared =
            LocalCursor.X * LocalCursor.X
            + LocalCursor.Y * LocalCursor.Y;

        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTransform = CandidateTransform;
            SelectedPassage = Passage;
            bHasValidZonePosition = true;
        }
    }

    PreviewActor->SetActorHiddenInGame(!bHasValidZonePosition);

    if (bHasValidZonePosition)
    {
        PreviewActor->SetActorTransform(BestTransform);
    }
}
void URICConstructionComponent::HidePassageHighlights()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    for (TActorIterator<ARICPassage> It(World); It; ++It)
    {
        It->SetHighlighted(false);
    }
}
