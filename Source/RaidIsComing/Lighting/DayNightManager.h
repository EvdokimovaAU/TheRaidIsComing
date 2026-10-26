#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightManager.generated.h"

class ADirectionalLight;
class ASkyLight;
class AWaveManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRICNightStateChanged, bool, bNight);

UCLASS(Blueprintable)
class RAIDISCOMING_API ADayNightManager : public AActor
{
    GENERATED_BODY()

public:
    ADayNightManager();
    virtual void Tick(float DeltaSeconds) override;

    // Вызывается внешней системой при смене волны, а не каждый кадр.
    UFUNCTION(BlueprintCallable, Category = "DayNight")
    void UpdateWaveState(int32 NewCurrentWave, int32 NewTotalWaves);

    UFUNCTION(BlueprintPure, Category = "DayNight")
    bool IsNight() const { return bIsNight; }

    UFUNCTION(BlueprintPure, Category = "DayNight")
    float GetDarknessProgress() const { return DarknessProgress; }

    UPROPERTY(BlueprintAssignable, Category = "DayNight")
    FRICNightStateChanged OnNightStateChanged;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Waves", meta = (ClampMin = "0"))
    int32 CurrentWave = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Waves", meta = (ClampMin = "1"))
    int32 TotalWaves = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Waves", meta = (ClampMin = "1"))
    int32 DarknessStartWave = 3;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DayNight|State")
    float DarknessProgress = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DayNight|State")
    bool bIsNight = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Transition", meta = (ClampMin = "0.01", Units = "s"))
    float TransitionDuration = 3.0f;

    // Optional: assign the level's existing wave manager.
    // Without it UpdateWaveState remains available for manual/external control.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|Waves")
    TObjectPtr<AWaveManager> WaveManager;

    // Назначаются пипеткой на экземпляре менеджера в уровне.
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|Lights")
    TObjectPtr<ADirectionalLight> SunLight;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DayNight|Lights")
    TObjectPtr<ASkyLight> SkyLight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun", meta = (ClampMin = "0"))
    float DaySunIntensity = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun", meta = (ClampMin = "0"))
    float TwilightSunIntensity = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun", meta = (ClampMin = "0"))
    float NightSunIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    FLinearColor DaySunColor = FLinearColor(1.0f, 0.95f, 0.85f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    FLinearColor TwilightSunColor = FLinearColor(1.0f, 0.35f, 0.12f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    FLinearColor NightSunColor = FLinearColor(0.2f, 0.3f, 0.6f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    float DaySunPitch = -60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    float TwilightSunPitch = -10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
    float NightSunPitch = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky", meta = (ClampMin = "0"))
    float DaySkyIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky", meta = (ClampMin = "0"))
    float TwilightSkyIntensity = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky", meta = (ClampMin = "0"))
    float NightSkyIntensity = 0.03f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleWaveStarted(int32 WaveNumber);

    UFUNCTION()
    void HandleWaveCompleted(int32 WaveNumber);

    // Reuses the existing smooth transition for night and dawn.
    void StartDarknessTransition(float NewTarget);

    float StartDarkness = 0.0f;
    float TargetDarkness = 0.0f;
    float TransitionElapsed = 0.0f;
    float ActiveDuration = 3.0f;
    float SunYaw = 0.0f;

    float CalculateTargetDarkness() const;
    void ApplyLighting();
    void RefreshNightState();
};
