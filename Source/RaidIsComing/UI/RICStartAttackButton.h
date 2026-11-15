#pragma once

#include "Components/Button.h"
#include "RICStartAttackButton.generated.h"

UCLASS(meta = (DisplayName = "Start Attack Button"))
class RAIDISCOMING_API URICStartAttackButton : public UButton
{
    GENERATED_BODY()

public:
    URICStartAttackButton(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
    virtual const FText GetPaletteCategory() override;
#endif

protected:
    virtual void OnWidgetRebuilt() override;
};
