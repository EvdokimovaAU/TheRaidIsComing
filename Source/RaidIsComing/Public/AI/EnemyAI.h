
#pragma once

#include "CoreMinimal.h"
#include "AI/AIBase.h"
#include "TimerManager.h"
#include "EnemyAI.generated.h"

class AEnemyAI;

// Event fired once when an enemy dies
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

    // ======================================
    // EVENTS
    // ======================================

    UPROPERTY(BlueprintAssignable, Category = "AI|Events")
    FOnEnemyDied OnEnemyDied;


    // ======================================
    // TARGET
    // ======================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Target")
    TObjectPtr<AActor> Capitol = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Target")
    TObjectPtr<AActor> CurrentTarget = nullptr;


    // ======================================
    // NAVIGATION
    // ======================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Navigation")
    float RouteProgress = 0.0f;


    // ======================================
    // MOVEMENT SETTINGS
    // ======================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement",
        meta = (ClampMin = "1.0"))
    float AcceptanceRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement",
        meta = (ClampMin = "100.0"))
    float NavigationSearchRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    float ObstacleDetectionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    float MovementSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
    bool bAutoStartMovement = true;


    // ======================================
    // TARGET METHODS
    // ======================================

    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void SetCapitol(AActor* NewCapitol);

    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AActor* GetCurrentTarget() const;


    // ======================================
    // MOVEMENT METHODS
    // ======================================

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void MoveToTarget();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void StopMovement();

    UFUNCTION(BlueprintCallable, Category = "AI|Movement")
    virtual void OnObstacleDetected(AActor* Obstacle);


    // ======================================
    // DEATH
    // ======================================

    virtual void Die() override;


protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;


private:

    FTimerHandle MovementStartTimer;

    void TryStartMovement();
};
