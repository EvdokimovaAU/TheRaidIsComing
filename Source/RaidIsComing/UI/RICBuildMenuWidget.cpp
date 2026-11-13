#include "RICBuildMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Layout/ArrangedChildren.h"
#include "Construction/RICBuilding.h"
#include "InputCoreTypes.h"

void URICBuildMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (Button_ArcherTower)
    {
        Button_ArcherTower->OnClicked.AddUniqueDynamic(this, &ThisClass::SelectArcher);
    }
    if (Button_MageTower)
    {
        Button_MageTower->OnClicked.AddUniqueDynamic(this, &ThisClass::SelectMage);
    }
}

void URICBuildMenuWidget::RefreshCosts(
    TSubclassOf<ARICBuilding> ArcherClass, TSubclassOf<ARICBuilding> MageClass)
{
    const ARICBuilding* Archer = ArcherClass.GetDefaultObject();
    const ARICBuilding* Mage = MageClass.GetDefaultObject();
    if (Text_ArcherTower)
        Text_ArcherTower->SetText(Archer ? FText::AsNumber(Archer->GetConstructionCost(this)) : FText::GetEmpty());
    if (Text_MageTower)
        Text_MageTower->SetText(Mage ? FText::AsNumber(Mage->GetConstructionCost(this)) : FText::GetEmpty());
    if (Button_ArcherTower) Button_ArcherTower->SetIsEnabled(Archer != nullptr);
    if (Button_MageTower) Button_MageTower->SetIsEnabled(Mage != nullptr);
}

FBox2D URICBuildMenuWidget::GetMenuBounds(const FVector2D& LayoutSize) const
{
    FBox2D Bounds(ForceInit);
    // Пустые отступы Canvas не входят в видимые границы меню.
    if (WidgetTree && Cast<UCanvasPanel>(WidgetTree->RootWidget))
    {
        FArrangedChildren Children(EVisibility::Visible);
        WidgetTree->RootWidget->TakeWidget()->ArrangeChildren(
            FGeometry::MakeRoot(LayoutSize, FSlateLayoutTransform()), Children);
        for (int32 Index = 0; Index < Children.Num(); ++Index)
        {
            const FSlateRect Rect = Children[Index].Geometry.GetLayoutBoundingRect();
            Bounds += FVector2D(Rect.Left, Rect.Top);
            Bounds += FVector2D(Rect.Right, Rect.Bottom);
        }
    }
    return Bounds.bIsValid ? Bounds : FBox2D(FVector2D::ZeroVector, LayoutSize);
}

FReply URICBuildMenuWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::RightMouseButton)
    {
        OnCancelled.Broadcast();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void URICBuildMenuWidget::SelectArcher() { OnTowerSelected.Broadcast(false); }
void URICBuildMenuWidget::SelectMage() { OnTowerSelected.Broadcast(true); }
