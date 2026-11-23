#include "DayNightManager.h"
#include "AI/WaveManager.h"

#include "Components/SceneComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    template<typename T>
    T BlendDayTwilightNight(const T& Day, const T& Twilight, const T& Night, float Progress)
    {
        const bool bBeforeTwilight = Progress <= 0.5f;
        const float Alpha = bBeforeTwilight ? Progress * 2.0f : (Progress - 0.5f) * 2.0f;
        const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
        return bBeforeTwilight ? FMath::Lerp(Day, Twilight, SmoothAlpha)
                              : FMath::Lerp(Twilight, Night, SmoothAlpha);
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
    bIsFirstLevel = UGameplayStatics::GetCurrentLevelName(this) == TEXT("Level_1");

    if (IsValid(WaveManager.Get()))
    {
        TotalWaves = FMath::Max(0, WaveManager->GetTotalWaves());
        CurrentWave = FMath::Clamp(WaveManager->GetCurrentWaveNumber(), 0, TotalWaves);
    }
    else
    {
        TotalWaves = FMath::Max(0, TotalWaves);
        CurrentWave = FMath::Clamp(CurrentWave, 0, TotalWaves);
    }

    if (IsValid(SunLight.Get()))
    {
        SunYaw = SunLight->GetActorRotation().Yaw;
        if (SunLight->GetLightComponent()
            && SunLight->GetLightComponent()->Mobility != EComponentMobility::Movable)
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
    else if (SkyLight->GetLightComponent()
        && SkyLight->GetLightComponent()->Mobility != EComponentMobility::Movable)
    {
        UE_LOG(LogTemp, Warning, TEXT("DayNight: set SkyLight mobility to Movable."));
    }

    // Apply the configured initial state immediately (including permanent
    // night). No forced daylight frame before the first transition.
    TargetDarkness = GetInitialDarkness();
    DarknessProgress = TargetDarkness;
    bLightingInitialized = true;
    ApplyLighting();
    RefreshNightState();
    SetActorTickEnabled(false);

    if (IsValid(WaveManager.Get()))
    {
        WaveManager->OnWaveStarted.AddUniqueDynamic(
            this, &ADayNightManager::HandleWaveStarted);

        UE_LOG(LogTemp, Log, TEXT("DayNight %s: connected to wave manager %s."),
            *GetName(), *GetNameSafe(WaveManager.Get()));
    }
    else if (LightingMode == ERICLightingMode::ByWaves)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("DayNight: WaveManager is not assigned; using manual UpdateWaveState control."));
    }

    // Also catches up when BeginPlay happens between waves or after the last
    // wave. Lighting progress no longer depends on the active/completed state.
    StartDarknessTransition(CalculateTargetDarkness());
}

void ADayNightManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(WaveManager.Get()))
    {
        WaveManager->OnWaveStarted.RemoveDynamic(
            this, &ADayNightManager::HandleWaveStarted);
    }
    Super::EndPlay(EndPlayReason);
}

void ADayNightManager::HandleWaveStarted(int32 WaveNumber)
{
    if (!IsValid(WaveManager.Get()))
    {
        return;
    }

    UpdateWaveState(WaveNumber, WaveManager->GetTotalWaves());
}

float ADayNightManager::GetInitialDarkness() const
{
    if (LightingMode == ERICLightingMode::PermanentNight)
    {
        return 1.0f;
    }
    return FMath::IsFinite(InitialDarkness)
        ? FMath::Clamp(InitialDarkness, 0.0f, 1.0f) : 0.0f;
}

bool ADayNightManager::IsNight() const
{
    // A searchlight can begin play before this actor. Give it the configured
    // initial night state instead of the not-yet-initialized bool.
    return bLightingInitialized ? bIsNight : GetInitialDarkness() >= 1.0f;
}
float ADayNightManager::CalculateTargetDarkness() const
{
    if (LightingMode == ERICLightingMode::PermanentNight)
    {
        return 1.0f;
    }

    const float Initial = GetInitialDarkness();

    if (TotalWaves <= 0 || CurrentWave <= 0)
    {
        return Initial;
    }

    if (NightWave > 0)
    {
        if (CurrentWave >= NightWave) return 1.0f;
        return CurrentWave == NightWave - 1 ? FMath::Max(Initial, 0.5f) : Initial;
    }

    const int32 DayWaves = TotalWaves / 3;

    if (CurrentWave <= DayWaves)
    {
        return Initial;
    }

    const float Final = FMath::Clamp(FinalDarkness, 0.0f, 1.0f);
    const int32 FirstNightWave = FMath::Max(DayWaves + 2, 2 * TotalWaves / 3 + 1);
    return CurrentWave < FirstNightWave ? FMath::Clamp(0.5f, Initial, FMath::Max(Initial, Final)) : Final;
}

void ADayNightManager::UpdateWaveState(int32 NewCurrentWave, int32 NewTotalWaves)
{
    TotalWaves = FMath::Max(0, NewTotalWaves);
    CurrentWave = FMath::Clamp(NewCurrentWave, 0, TotalWaves);
    const float NewTarget = CalculateTargetDarkness();

    UE_LOG(LogTemp, Log, TEXT("DayNight: wave %d/%d, target darkness %.2f."),
        CurrentWave, TotalWaves, NewTarget);

    StartDarknessTransition(NewTarget);
}

void ADayNightManager::StartDarknessTransition(float NewTarget)
{
    NewTarget = FMath::IsFinite(NewTarget)
        ? FMath::Clamp(NewTarget, 0.0f, 1.0f) : GetInitialDarkness();

    // Avoid restarting the same transition.
    if (NewTarget == TargetDarkness)
    {
        return;
    }

    StartDarkness = DarknessProgress;
    TargetDarkness = NewTarget;
    TransitionElapsed = 0.0f;
    ActiveDuration = FMath::IsFinite(TransitionDuration)
        ? FMath::Max(8.0f, TransitionDuration) : 8.0f;
    if (bIsFirstLevel && TargetDarkness >= 1.0f)
    {
        ActiveDuration = 4.0f;
    }
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
    // On the first level the searchlight becomes usable during late dusk.
    const float NightThreshold = bIsFirstLevel && TargetDarkness >= 1.0f ? 0.8f : 1.0f;
    const bool bNewNight = DarknessProgress >= NightThreshold;
    if (bNewNight != bIsNight)
    {
        bIsNight = bNewNight;
        UE_LOG(LogTemp, Log, TEXT("DayNight: full night = %s."), bIsNight ? TEXT("Yes") : TEXT("No"));
        OnNightStateChanged.Broadcast(bIsNight);
    }
}

void ADayNightManager::ApplyLighting()
{
    const float Brightness = FMath::Lerp(FMath::Max(0.0f, DayBrightness),
        FMath::Max(0.0f, NightBrightness), DarknessProgress);

    if (IsValid(SunLight.Get()))
    {
        UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent());
        if (IsValid(Light))
        {
            Light->SetIntensity(Brightness * FMath::Max(0.0f, BlendDayTwilightNight(
                DaySunIntensity * 1.25f, TwilightSunIntensity * 1.2f, NightSunIntensity, DarknessProgress)));
            Light->SetLightColor(BlendDayTwilightNight(
                DaySunColor, TwilightSunColor, NightSunColor, DarknessProgress));
            const float Pitch = BlendDayTwilightNight(
                DaySunPitch, TwilightSunPitch, NightSunPitch, DarknessProgress);
            SunLight->SetActorRotation(FRotator(Pitch, SunYaw, 0.0f));
        }
    }

    if (IsValid(SkyLight.Get()))
    {
        USkyLightComponent* SkyComponent = SkyLight->GetLightComponent();

        if (IsValid(SkyComponent))
        {
            const float DesiredIntensity = Brightness * FMath::Max(
                0.0f,
                BlendDayTwilightNight(
                    DaySkyIntensity * 1.25f,
                    TwilightSkyIntensity * 1.2f,
                    NightSkyIntensity,
                    DarknessProgress));

            SkyComponent->SetIntensity(DesiredIntensity);

            if (DarknessProgress >= 1.0f)
            {
                UE_LOG(
                    LogTemp,
                    Warning,
                    TEXT("[NightDebug] SkyLight=%s | Desired=%.3f | Actual=%.3f | Visible=%d | AffectsWorld=%d | Movable=%d"),
                    *GetNameSafe(SkyLight.Get()),
                    DesiredIntensity,
                    SkyComponent->Intensity,
                    SkyComponent->IsVisible() ? 1 : 0,
                    SkyComponent->bAffectsWorld ? 1 : 0,
                    SkyComponent->Mobility == EComponentMobility::Movable ? 1 : 0);
            }
        }
    }
    else if (DarknessProgress >= 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[NightDebug] SkyLight is NOT assigned"));
    }
}
