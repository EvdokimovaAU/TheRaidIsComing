#include "UI/RICStartAttackButton.h"
#include "RaidIsComingPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Brushes/SlateNoResource.h"
#include "Components/CanvasPanelSlot.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

URICStartAttackButton::URICStartAttackButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InitIsFocusable(false);
    static ConstructorHelpers::FObjectFinder<UObject> Sword(
        TEXT("/Game/Assets/UI/Buttons/Menu_Sprite_7.Menu_Sprite_7"));
    SwordIcon.SetResourceObject(Sword.Object);
}

#if WITH_EDITOR
const FText URICStartAttackButton::GetPaletteCategory()
{
    return NSLOCTEXT("Waves", "PaletteCategory", "Raid Is Coming");
}
#endif

void URICStartAttackButton::OnWidgetRebuilt()
{
    Super::OnWidgetRebuilt();

    if (SwordIcon.GetResourceObject() && MyButton.IsValid())
    {
        if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
        {
            CanvasSlot->SetAutoSize(true);
        }
        FButtonStyle Style = GetStyle();
        Style.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource())
            .SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource())
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
        SetStyle(Style);
        MyButton->SetHAlign(HAlign_Center);
        MyButton->SetVAlign(VAlign_Center);
        MyButton->SetContent(
            SNew(SBox).WidthOverride(88.f).HeightOverride(88.f)
            [
                SNew(SImage).Image(&SwordIcon)
            ]);
        SetToolTipText(NSLOCTEXT("Waves", "StartAttack", "Начать бой"));
    }

    if (IsDesignTime())
    {
        return;
    }

    if (ARaidIsComingPlayerController* Controller = GetOwningPlayer<ARaidIsComingPlayerController>())
    {
        Controller->RegisterStartAttackButton(this);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}
