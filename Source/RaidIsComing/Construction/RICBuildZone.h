#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RICBuildTypes.h"
#include "RICBuildZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class RAIDISCOMING_API ARICBuildZone : public AActor
{
    GENERATED_BODY()

public:
    ARICBuildZone();

    virtual void OnConstruction(
        const FTransform& Transform
    ) override;

    // Границы зоны для будущей проверки размещения.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UBoxComponent> ZoneBounds;

    // Плоскость, на которую назначим зелёный материал.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UStaticMeshComponent> HighlightMesh;

    // Половина размера зоны в сантиметрах.
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Build Zone",
        meta = (ClampMin = "1.0")
    )
    FVector2D HalfSize = FVector2D(400.0, 300.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Zone")
    bool bAllowBarracks = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Zone")
    bool bAllowArcherTowers = true;

    UFUNCTION(BlueprintCallable, Category = "Build Zone")
    void SetHighlighted(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Build Zone")
    bool IsBuildingAllowed(ERICBuildingType Type) const;

protected:
    virtual void BeginPlay() override;
};