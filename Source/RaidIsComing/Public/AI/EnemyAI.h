
#pragma once

#include "CoreMinimal.h"
#include "AI/AIBase.h"
#include "TimerManager.h"
#include "EnemyAI.generated.h"

class AEnemyAI;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnEnemyDied,
    AEnemyAI*,
    DeadEnemy
);

UCLASS()
class RAIDISCOMING_API AEnemyAI : public AAIBase
{
    GENERATED_BODY()

public:
    AEnemyAI();

    UPROPERTY(BlueprintAssignable, Category = "AI|Events")
    FOnEnemyDied OnEnemyDied;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "AI|Rewards",
        meta = (ClampMin = "0", UIMin = "0")
    )
    int32 KillReward = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Target")
    TObjectPtr<AActor> Capitol = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Target")
    TObjectPtr<AActor> CurrentTarget = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Navigation")
    float RouteProgress = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Movement",
        meta = (ClampMin = "1.0")
    )
    float AcceptanceRadius = 100.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Movement",
        meta = (ClampMin = "100.0")
    )
    float NavigationSearchRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    float ObstacleDetectionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    float MovementSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    bool bAutoStartMovement = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Death",
        meta = (ClampMin = "0.0")
    )
    float DeathDestroyDelay = 0.1f;

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

    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle MovementStartTimer;
    FTimerHandle NavigationRetryTimer;

    void ScheduleNavigationRetry();
    void TryStartMovement();
    void GrantKillReward();

    bool bKillRewardGranted = false;
};
