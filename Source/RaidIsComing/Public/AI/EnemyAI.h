#pragma once

#include "CoreMinimal.h"
#include "AI/AIBase.h"
#include "EnemyAI.generated.h"

UCLASS()
class RAIDISCOMING_API AEnemyAI : public AAIBase
{
    GENERATED_BODY()

public:
    AEnemyAI();

    // Основная цель врагов — Капитолий
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Target")
    TObjectPtr<AActor> Capitol = nullptr;

    // Объект, к которому враг направляется сейчас
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Target")
    TObjectPtr<AActor> CurrentTarget = nullptr;

    // Прогресс врага по маршруту (позже подключим навигацию)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Navigation")
    float RouteProgress = 0.0f;

    // Радиус обнаружения препятствий
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    float ObstacleDetectionRadius = 150.0f;

    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void SetCapitol(AActor* NewCapitol);

    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AActor* GetCurrentTarget() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void MoveToTarget();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void StopMovement();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void OnObstacleDetected(AActor* Obstacle);

protected:
    virtual void BeginPlay() override;
};
