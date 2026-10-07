#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RICBuildTypes.h"
#include "RICBuildZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ERICBuildZoneShape : uint8
{
    Rectangle  UMETA(DisplayName = "Rectangle"),
    RingSector UMETA(DisplayName = "Ring Sector")
};

UCLASS(Blueprintable)
class RAIDISCOMING_API ARICBuildZone : public AActor
{
    GENERATED_BODY()

public:
    ARICBuildZone();

    virtual void OnConstruction(const FTransform& Transform) override;

    // Вспомогательная рамка. Не участвует в коллизиях.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UBoxComponent> ZoneBounds;

    // Существующий Plane для прямоугольного варианта.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UStaticMeshComponent> HighlightMesh;

    // Генерируемая поверхность кольцевого сектора.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UProceduralMeshComponent> SectorHighlightMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Zone")
    ERICBuildZoneShape ZoneShape = ERICBuildZoneShape::Rectangle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Build Zone")
    TObjectPtr<UMaterialInterface> HighlightMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Rectangle",
        meta = (
            ClampMin = "1.0",
            EditCondition = "ZoneShape == ERICBuildZoneShape::Rectangle",
            EditConditionHides
            ))
    FVector2D HalfSize = FVector2D(400.0, 300.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Ring Sector",
        meta = (
            ClampMin = "1.0",
            EditCondition = "ZoneShape == ERICBuildZoneShape::RingSector",
            EditConditionHides
            ))
    float InnerRadius = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Ring Sector",
        meta = (
            ClampMin = "2.0",
            EditCondition = "ZoneShape == ERICBuildZoneShape::RingSector",
            EditConditionHides
            ))
    float OuterRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Ring Sector",
        meta = (
            EditCondition = "ZoneShape == ERICBuildZoneShape::RingSector",
            EditConditionHides
            ))
    float StartAngleDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Ring Sector",
        meta = (
            ClampMin = "1.0",
            ClampMax = "360.0",
            EditCondition = "ZoneShape == ERICBuildZoneShape::RingSector",
            EditConditionHides
            ))
    float ArcAngleDegrees = 120.0f;

    // Количество отрезков на полный круг.
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Build Zone|Ring Sector",
        meta = (
            ClampMin = "12",
            ClampMax = "512",
            EditCondition = "ZoneShape == ERICBuildZoneShape::RingSector",
            EditConditionHides
            ))
    int32 SegmentsPerCircle = 128;

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

private:
    // В редакторе форма видна. При запуске игры скрываем её.
    bool bIsHighlighted = true;

    void RebuildSectorMesh();
    void RefreshHighlightVisibility();
};