
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "ArcherTowerAI.generated.h"

UCLASS()
class RAIDISCOMING_API AArcherTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:
    AArcherTowerAI();

    // Интервал проверки противников
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Archer")
    float TargetCheckInterval = 0.2f;

    // Выполнить атаку
    virtual void Attack() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    // Таймер проверки целей
    FTimerHandle TargetCheckTimer;

    // Основной цикл башни
    void UpdateTower();
};
