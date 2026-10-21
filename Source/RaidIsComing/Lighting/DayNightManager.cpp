#include "DayNightManager.h"

#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"

namespace
{
    template<typename T>
    T BlendDayTwilightNight(const T& Day, const T& Twilight, const T& Night, float Progress)
    {
        return Progress <= 0.5f
            ? FMath::Lerp(Day, Twilight, Progress * 2.0f)
            : FMath::Lerp(Twilight, Night, (Progress - 0.5f) * 2.0f);
    }
}

ADayNightManager::ADayNightManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADayNightManager::BeginPlay()
{
    Super::BeginPlay();

    TotalWaves = FMath::Max(1, TotalWaves);
    CurrentWave = FMath::Clamp(CurrentWave, 0, TotalWaves);
    DarknessStartWave = FMath::Max(1, DarknessStartWave);

    if (IsValid(SunLight.Get()))
    {
        SunYaw = SunLight->GetActorRotation().Yaw;
        if (SunLight->GetComponent()->Mobility != EComponentMobility::Movable)
        {
            UE_LOG(LogTemp, Warning, TEXT("DayNight: set SunLight mobility to Movable."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DayNight: assign SunLight on the manager in the level."));
    }

    if (!IsValid(SkyLight.Get()))
    {
        UE_LOG(LogTemp, Warning, TEXT("DayNight: assign SkyLight on the manager in the level."));
    }
    else if (SkyLight->GetLightComponent()->Mobility != EComponentMobility::Movable)
    {
        UE_LOG(LogTemp, Warning, TEXT("DayNight: set SkyLight mobility to Movable."));
    }

    // Стартовое состояние уровня применяется сразу; дальнейшие изменения плавные.
    TargetDarkness = CalculateTargetDarkness();
    DarknessProgress = TargetDarkness;
    ApplyLighting();
    RefreshNightState();
    SetActorTickEnabled(false);
}

float ADayNightManager::CalculateTargetDarkness() const
{
    const int32 SafeTotal = FMath::Max(1, TotalWaves);
    const int32 StartWave = FMath::Max(1, DarknessStartWave);
    if (CurrentWave < StartWave || StartWave > SafeTotal)
    {
        return 0.0f;
    }

    // При Start=3, Total=6: волны 0..2 -> 0; 3 -> .25; 4 -> .5; 5 -> .75; 6 -> 1.
    const double Numerator = static_cast<double>(CurrentWave) - StartWave + 1.0;
    const double Denominator = static_cast<double>(SafeTotal) - StartWave + 1.0;
    return static_cast<float>(FMath::Clamp(Numerator / Denominator, 0.0, 1.0));
}

void ADayNightManager::UpdateWaveState(int32 NewCurrentWave, int32 NewTotalWaves)
{
    TotalWaves = FMath::Max(1, NewTotalWaves);
    CurrentWave = FMath::Clamp(NewCurrentWave, 0, TotalWaves);
    const float NewTarget = CalculateTargetDarkness();

    UE_LOG(LogTemp, Log, TEXT("DayNight: wave %d/%d, target darkness %.2f."),
        CurrentWave, TotalWaves, NewTarget);

    // Повторное уведомление о той же волне не перезапускает переход.
    if (NewTarget == TargetDarkness)
    {
        return;
    }

    StartDarkness = DarknessProgress;
    TargetDarkness = NewTarget;
    TransitionElapsed = 0.0f;
    ActiveDuration = FMath::IsFinite(TransitionDuration)
        ? FMath::Max(0.01f, TransitionDuration) : 3.0f;
    SetActorTickEnabled(true);
}

void ADayNightManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TransitionElapsed += FMath::Max(0.0f, DeltaSeconds);
    const float Alpha = FMath::Clamp(TransitionElapsed / ActiveDuration, 0.0f, 1.0f);
    const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
    DarknessProgress = FMath::Lerp(StartDarkness, TargetDarkness, SmoothAlpha);

    if (Alpha >= 1.0f)
    {
        DarknessProgress = TargetDarkness;
        SetActorTickEnabled(false);
    }
    ApplyLighting();
    RefreshNightState();
}

void ADayNightManager::RefreshNightState()
{
    const bool bNewNight = DarknessProgress >= 1.0f;
    if (bNewNight != bIsNight)
    {
        bIsNight = bNewNight;
        UE_LOG(LogTemp, Log, TEXT("DayNight: full night = %s."), bIsNight ? TEXT("Yes") : TEXT("No"));
        OnNightStateChanged.Broadcast(bIsNight);
    }
}

void ADayNightManager::ApplyLighting()
{
    if (IsValid(SunLight.Get()))
    {
        UDirectionalLightComponent* Light = SunLight->GetComponent();
        Light->SetIntensity(FMath::Max(0.0f, BlendDayTwilightNight(
            DaySunIntensity, TwilightSunIntensity, NightSunIntensity, DarknessProgress)));
        Light->SetLightColor(BlendDayTwilightNight(
            DaySunColor, TwilightSunColor, NightSunColor, DarknessProgress));
        const float Pitch = BlendDayTwilightNight(
            DaySunPitch, TwilightSunPitch, NightSunPitch, DarknessProgress);
        SunLight->SetActorRotation(FRotator(Pitch, SunYaw, 0.0f));
    }

    if (IsValid(SkyLight.Get()))
    {
        SkyLight->GetLightComponent()->SetIntensity(FMath::Max(0.0f, BlendDayTwilightNight(
            DaySkyIntensity, TwilightSkyIntensity, NightSkyIntensity, DarknessProgress)));
    }
}
