#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "RICBuildTypes.h"
#include "RICConstructionComponent.generated.h"

class ARICBuilding;
class AStaticMeshActor;
class UMaterialInterface;

UCLASS(ClassGroup = (Construction), meta = (BlueprintSpawnableComponent))
class RAIDISCOMING_API URICConstructionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URICConstructionComponent();

    UFUNCTION(BlueprintCallable, Category = "Construction")
    bool StartPreview(TSubclassOf<ARICBuilding> BuildingClass);

    UFUNCTION(BlueprintCallable, Category = "Construction")
    void CancelPreview();

    UFUNCTION(BlueprintPure, Category = "Construction")
    bool IsBuildingMode() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Construction")
    TObjectPtr<UMaterialInterface> PreviewMaterial;

protected:
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<AStaticMeshActor> PreviewActor;

    ERICBuildingType SelectedType = ERICBuildingType::Barracks;

    // Расстояние от нижней точки меша до его pivot.
    float PreviewHeightOffset = 0.0f;

    void UpdatePreviewPosition();
    void UpdateZoneHighlights(bool bEnabled);
};