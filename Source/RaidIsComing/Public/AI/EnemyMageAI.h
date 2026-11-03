
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "EnemyMageAI.generated.h"

class AEnemyBarrier;
class AEnemySwordsmanAI;

UENUM(BlueprintType)
enum class EMageState : uint8
{
    CreatingBarrier,
    FollowingSwordsmen,
    MovingAlone,
    WaitingForBarrier,
    Attacking
};

UCLASS()
class RAIDISCOMING_API AEnemyMageAI : public AEnemyAI
{
    GENERATED_BODY()

public:
    AEnemyMageAI();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float AllyDetectionRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierCooldown = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TSubclassOf<AEnemyBarrier> BarrierClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TObjectPtr<AEnemyBarrier> ActiveBarrier = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    EMageState MageState = EMageState::MovingAlone;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TArray<TObjectPtr<AEnemyAI>> AlliesAhead;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageMovementSpeed = 380.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float NormalMovementSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageAcceleration = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageBrakingDeceleration = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float SpeedChangeRate = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowUpdateInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float RepathDistance = 100.0f;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void FindAlliesAhead();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    bool CanCreateBarrier() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void Barrier();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void RemoveBarrier();

    virtual void MoveToTarget() override;
    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle BehaviorTimer;

    TWeakObjectPtr<AEnemyAI> FollowedSwordsman;
    TWeakObjectPtr<AActor> CurrentMoveTarget;

    bool bMovingToCapitol = false;
    bool bFollowingSwordsman = false;

    float NextBarrierTime = 0.0f;
    float CurrentDesiredSpeed = 300.0f;

    void UpdateBehavior();
    void SelectSwordsman();
    void UpdateMovement();
    void UpdateFollowSpeed(float DeltaTime);

    bool HasActiveBarrier() const;
    bool IsSwordsman(const AEnemyAI* Enemy) const;
};
