#include "UI/RICTimeOfDayWidget.h"

#include "Components/Image.h"
#include "EngineUtils.h"
#include "Lighting/DayNightManager.h"

void URICTimeOfDayWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (TActorIterator<ADayNightManager> Manager(GetWorld()); Manager)
    {
        Lighting = *Manager;
        NightBlend = Manager->GetDarknessProgress();
    }
    ApplyImageBlend();
}

void URICTimeOfDayWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    if (const ADayNightManager* Manager = Lighting.Get())
    {
        const float Darkness = Manager->GetDarknessProgress();
        if (NightBlend != Darkness)
        {
            NightBlend = Darkness;
            ApplyImageBlend();
        }
    }
}

void URICTimeOfDayWidget::ApplyImageBlend()
{
    for (const FName Name : { FName(TEXT("DaySky")), FName(TEXT("NightSky")),
        FName(TEXT("Sun")), FName(TEXT("Moon")), FName(TEXT("SunsetSky")) })
    {
        if (auto* Image = Cast<UImage>(GetWidgetFromName(Name)))
        {
            const float DaySkyAlpha = FMath::Max(0.0f, 1.0f - 2.0f * NightBlend);
            const float NightSkyAlpha = FMath::Max(0.0f, 2.0f * NightBlend - 1.0f);
            float Alpha = 1.0f - NightBlend;
            if (Name == TEXT("DaySky")) Alpha = DaySkyAlpha;
            if (Name == TEXT("NightSky")) Alpha = NightSkyAlpha;
            if (Name == TEXT("SunsetSky")) Alpha = 1.0f - DaySkyAlpha - NightSkyAlpha;
            if (Name == TEXT("Moon")) Alpha = NightBlend;

            Image->SetRenderTranslation(FVector2D::ZeroVector);
            FLinearColor Tint = Image->GetColorAndOpacity();
            Tint.A = 1.0f;
            Image->SetColorAndOpacity(Tint);
            Image->SetRenderOpacity(Alpha);
            Image->SetVisibility(Alpha > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
    }
}