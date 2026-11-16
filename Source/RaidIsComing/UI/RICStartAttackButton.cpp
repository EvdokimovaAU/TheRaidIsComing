#include "UI/RICStartAttackButton.h"
#include "RaidIsComingPlayerController.h"

URICStartAttackButton::URICStartAttackButton(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    InitIsFocusable(false);
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
