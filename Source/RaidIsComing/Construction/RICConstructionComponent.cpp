#include "RICConstructionComponent.h"

#include "RICBuilding.h"
#include "RICBuildZone.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

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

    if (!Controller || !Controller->IsLocalController()
        || !GetWorld() || !BuildingClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Cannot start preview: controller or building class is missing."));
        return false;
    }

    const ARICBuilding* BuildingDefaults =
        BuildingClass->GetDefaultObject<ARICBuilding>();

    const UStaticMeshComponent* SourceMesh =
        BuildingDefaults->BuildingMesh.Get();

    if (!SourceMesh || !SourceMesh->GetStaticMesh())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Cannot start preview: BuildingMesh has no Static Mesh."));
        return false;
    }

    // Преграды позже получат отдельный режим выбора прохода.
    if (BuildingDefaults->BuildingType == ERICBuildingType::Barrier)
    {
        return false;
    }

    CancelPreview();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = Controller;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(),
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParameters
    );

    if (!IsValid(PreviewActor.Get()))
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

    PreviewActor->SetActorScale3D(
        SourceMesh->GetRelativeScale3D());

    for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Material = PreviewMaterial.Get();

        if (!Material)
        {
            Material = SourceMesh->GetMaterial(Index);
        }

        Mesh->SetMaterial(Index, Material);
    }

    // Поднимаем меш так, чтобы его низ стоял на поверхности.
    // Это учитывает и куб с pivot в центре.
    const FBoxSphereBounds Bounds =
        Mesh->CalcBounds(Mesh->GetComponentTransform());

    PreviewHeightOffset =
        PreviewActor->GetActorLocation().Z
        - (Bounds.Origin.Z - Bounds.BoxExtent.Z);

    SelectedType = BuildingDefaults->BuildingType;

    UpdateZoneHighlights(true);
    SetComponentTickEnabled(true);
    UpdatePreviewPosition();

    return true;
}

void URICConstructionComponent::CancelPreview()
{
    SetComponentTickEnabled(false);

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

void URICConstructionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsBuildingMode())
    {
        CancelPreview();
        return;
    }

    UpdatePreviewPosition();
}

void URICConstructionComponent::UpdatePreviewPosition()
{
    APlayerController* Controller =
        Cast<APlayerController>(GetOwner());

    if (!Controller || !IsBuildingMode() || !GetWorld())
    {
        return;
    }

    FVector RayOrigin;
    FVector RayDirection;

    if (!Controller->DeprojectMousePositionToWorld(
        RayOrigin, RayDirection))
    {
        PreviewActor->SetActorHiddenInGame(true);
        return;
    }

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = true;
    QueryParams.AddIgnoredActor(PreviewActor.Get());

    if (APawn* Pawn = Controller->GetPawn())
    {
        QueryParams.AddIgnoredActor(Pawn);
    }

    FHitResult Hit;

    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        RayOrigin,
        RayOrigin + RayDirection * 100000.0f,
        ECC_Visibility,
        QueryParams
    );

    if (!bHit)
    {
        PreviewActor->SetActorHiddenInGame(true);
        return;
    }

    const FVector Position =
        Hit.ImpactPoint + FVector(0.0, 0.0, PreviewHeightOffset);

    PreviewActor->SetActorLocation(Position);
    PreviewActor->SetActorHiddenInGame(false);
}

void URICConstructionComponent::UpdateZoneHighlights(bool bEnabled)
{
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<ARICBuildZone> It(GetWorld()); It; ++It)
    {
        ARICBuildZone* Zone = *It;

        Zone->SetHighlighted(
            bEnabled && Zone->IsBuildingAllowed(SelectedType));
    }
}

void URICConstructionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    CancelPreview();
    Super::EndPlay(EndPlayReason);
}