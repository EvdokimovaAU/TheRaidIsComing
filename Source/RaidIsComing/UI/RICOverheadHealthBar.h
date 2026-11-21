#pragma once

#include "Components/WidgetComponent.h"
#include "RICOverheadHealthBar.generated.h"

UCLASS()
class URICOverheadHealthBar : public UWidgetComponent
{
    GENERATED_BODY()

public:
    URICOverheadHealthBar();

    static void Attach(AActor* Owner, FVector WorldAnchor, FIntPoint Size,
        TAttribute<FVector2D> Health, bool bFriendly);

private:
    UPROPERTY()
    FSlateBrush Frame;
    UPROPERTY()
    FSlateBrush FriendlyFill;
    UPROPERTY()
    FSlateBrush EnemyFill;
};
