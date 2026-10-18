
#pragma once

#include "CoreMinimal.h"
#include "AIBase.h"
#include "PlayerAI.generated.h"

class AEnemyAI;

UCLASS()
class RAIDISCOMING_API APlayerAI : public AAIBase
{
    GENERATED_BODY()

public:

    APlayerAI();

    // Радиус обнаружения противников
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Detection")
    float DetectionRadius = 35.0f;

    // Поиск всех живых врагов в радиусе
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    TArray<AEnemyAI*> FindEnemiesInRadius();

    // Выбор приоритетного противника
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AEnemyAI* SelectTarget(const TArray<AEnemyAI*>& Enemies);

    // Найти и сохранить новую цель
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void FindTarget();

    // Реакция на смерть текущей цели
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void OnTargetDied();

protected:

    virtual void BeginPlay() override;
};
