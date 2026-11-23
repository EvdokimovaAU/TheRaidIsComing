#pragma once

#include "Blueprint/UserWidget.h"
#include "RICTimeOfDayWidget.generated.h"

class ADayNightManager;
class UWidgetAnimation;

UCLASS()
class URICTimeOfDayWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Time of Day", meta = (BindWidgetAnimOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetAnimation> StartNight;

    void ApplyImageBlend();
    float NightBlend = 0.0f;
    TWeakObjectPtr<ADayNightManager> Lighting;
};
