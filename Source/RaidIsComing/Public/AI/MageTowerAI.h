
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "MageTowerAI.generated.h"

UCLASS()
class RAIDISCOMING_API AMageTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:
    AMageTowerAI();

    // Радиус массового урона
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float SplashRadius = 200.0f;

    // Интервал проверки целей
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float TargetCheckInterval = 0.2f;

    // Магическая атака
    virtual void Attack() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    // Таймер проверки
    FTimerHandle TargetCheckTimer;

    // Основной цикл
    void UpdateTower();

    // Массовый урон
    void ApplySplashDamage(const FVector& ImpactLocation);
};
