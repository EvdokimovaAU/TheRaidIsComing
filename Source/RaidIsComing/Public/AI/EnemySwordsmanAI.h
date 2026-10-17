
#pragma once

#include "CoreMinimal.h"
#include "EnemyAI.h"
#include "EnemySwordsmanAI.generated.h"

UCLASS()
class RAIDISCOMING_API AEnemySwordsmanAI : public AEnemyAI
{
    GENERATED_BODY()

public:
    AEnemySwordsmanAI();

    // Время между ударами по стене
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallAttackCooldown = 1.0f;

    // Урон по стене за один удар
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallDamage = 10.0f;

    // Стена, которую сейчас атакуем
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Target")
    TObjectPtr<AActor> TargetWall = nullptr;

    // Реакция на обнаруженную стену
    virtual void OnObstacleDetected(AActor* Obstacle) override;

    // Начать атаку стены
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StartWallAttack(AActor* Wall);

    // Нанести удар по стене
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void AttackWall();

    // Остановить атаку
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StopWallAttack();

    // Реакция на разрушение стены
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void OnWallDestroyed();

protected:
    // Таймер атаки
    FTimerHandle WallAttackTimer;
};
