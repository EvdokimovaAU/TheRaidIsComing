#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "RICBuildTypes.h"
#include "RICConstructionComponent.generated.h"

class UStaticMeshComponent;
class ARICBuilding;
class AStaticMeshActor;
class UMaterialInterface;
class ARICPassage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRICPlacementRejected, const FText&, Reason);

UCLASS(
    ClassGroup = (Construction),
    meta = (BlueprintSpawnableComponent))
    class RAIDISCOMING_API URICConstructionComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    URICConstructionComponent();

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool StartPreview(TSubclassOf<ARICBuilding> BuildingClass);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    void CancelPreview();

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool TryPlacePreview();

    UFUNCTION(BlueprintPure, Category = "Construction")
    bool IsBuildingMode() const;

    UFUNCTION(BlueprintPure, Category = "Construction")
    bool HasValidZonePosition() const;

    AActor* GetConstructionTargetUnderCursor(
        TSubclassOf<ARICBuilding> TowerClass,
        TSubclassOf<ARICBuilding> BarrierClass) const;
    void SetPreviewHighlightTarget(AActor* Target);

    // Suspend the existing preview, keeping highlights. Returns the footprint center on the zone plane.
    bool FreezePreview(FVector& OutGroundPosition);
    bool TryPlaceAt(TSubclassOf<ARICBuilding> BuildingClass, const FVector& GroundPosition);

    UPROPERTY(BlueprintAssignable, Category = "Construction")
    FRICPlacementRejected OnPlacementRejected;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construction")
    TObjectPtr<UMaterialInterface> PreviewMaterial;

protected:
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

private:
    friend class FRICBuildMenuPlacementTest;
    TOptional<FVector> FixedGroundPosition;
    bool bPreviewFrozen = false;
    bool RejectPlacement(const FText& Reason);
    TWeakObjectPtr<ARICPassage> SelectedPassage;
    TWeakObjectPtr<AActor> PreviewHighlightTarget;

    void UpdateBarrierPreviewPosition();
    void HidePassageHighlights();

    bool GetBarrierPreviewTransform(
        const ARICPassage& Passage,
        FTransform& OutTransform) const;
    bool GetBarrierTransform(
        const ARICPassage& Passage,
        const UStaticMeshComponent& Mesh,
        const FVector& Scale,
        const FBoxSphereBounds& Bounds,
        FTransform& OutTransform) const;
    UPROPERTY(Transient)
    TObjectPtr<AStaticMeshActor> PreviewActor;

    UPROPERTY(Transient)
    TSubclassOf<ARICBuilding> SelectedBuildingClass;

    ERICBuildingType SelectedType = ERICBuildingType::Barracks;

    FVector2D FootprintHalfSize = FVector2D::ZeroVector;
    FVector PreviewCenterOffset = FVector::ZeroVector;

    double PreviewHeightOffset = 0.0;

    bool bHasValidZonePosition = false;

    void UpdatePreviewPosition();
    void UpdateZoneHighlights(bool bEnabled);
    bool HasBuildingOverlap(
        const UStaticMeshComponent* CandidateMesh,
        const FTransform& CandidateTransform,
        const ARICBuilding* IgnoredBuilding = nullptr) const;
};
