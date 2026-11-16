#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RICBuildMenuWidget.generated.h"

class UButton;
class UTextBlock;
class ARICBuilding;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRICBuildMenuSelection, bool, bMageTower);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRICBuildMenuCancelled);

UCLASS(Abstract, Blueprintable)
class RAIDISCOMING_API URICBuildMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshCosts(TSubclassOf<ARICBuilding> ArcherClass, TSubclassOf<ARICBuilding> MageClass);
    FBox2D GetMenuBounds(const FVector2D& LayoutSize) const;
    FRICBuildMenuSelection OnTowerSelected;
    FRICBuildMenuCancelled OnCancelled;

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_ArcherTower;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> Button_MageTower;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_ArcherTower;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> Text_MageTower;

    UFUNCTION()
    void SelectArcher();
    UFUNCTION()
    void SelectMage();
};
