
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "EnemyMageAI.generated.h"

class AEnemyBarrier;
class AEnemySwordsmanAI;
class ARICBuilding;
class UAnimMontage;

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
    float AllyDetectionRadius = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierCooldown = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierDuration = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TSubclassOf<AEnemyBarrier> BarrierClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    TObjectPtr<UAnimMontage> BarrierMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Animation")
    float BarrierSpawnDelay = 0.45f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TObjectPtr<AEnemyBarrier> ActiveBarrier = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    EMageState MageState = EMageState::MovingAlone;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TArray<TObjectPtr<AEnemyAI>> AlliesAhead;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowDistance = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageMovementSpeed = 320.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float NormalMovementSpeed = 250.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageAcceleration = 350.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float MageBrakingDeceleration = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float SpeedChangeRate = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float FollowUpdateInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement")
    float RepathDistance = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallDetectionRadius = 220.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallAttackDistance = 140.f;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void FindAlliesAhead();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    bool CanCreateBarrier() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void Barrier();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void SpawnBarrierFromAnimation();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void RemoveBarrier();

    UFUNCTION(BlueprintPure, Category = "AI|Magic")
    bool HasActiveBarrier() const;

    virtual void MoveToTarget() override;
    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle BehaviorTimer;
    FTimerHandle BarrierSpawnTimer;
    FTimerHandle BarrierCastEndTimer;
    FTimerHandle AttackTimer;

    TWeakObjectPtr<AEnemyAI> FollowedSwordsman;
    TWeakObjectPtr<AActor> CurrentMoveTarget;
    TWeakObjectPtr<ARICBuilding> CombatTarget;

    bool bMovingToCapitol = false;
    bool bFollowingSwordsman = false;
    bool bCastingBarrier = false;
    bool bBarrierSpawnedThisCast = false;
    bool bAttacking = false;
    bool bBarrierDestroyed = false;

    float NextBarrierTime = 0.f;
    float CurrentDesiredSpeed = 250.f;

    // Swordsmen waiting for this mage
    TArray<TWeakObjectPtr<AEnemySwordsmanAI>> WaitingSwordsmen;

    void StopNearbySwordsmen();
    void ReleaseWaitingSwordsmen();

    void UpdateBehavior();
    void SelectSwordsman();
    void UpdateMovement();
    void UpdateFollowSpeed(float DeltaTime);

    void UpdateCombat();
    ARICBuilding* FindCombatTarget() const;
    void StartCombat(ARICBuilding* TargetBuilding);
    void StopCombat();
    void PerformAttack();

    void SpawnBarrierInternal();
    void FinishBarrierCast();
    void CancelBarrierCast();
    void RemoveBarrierInternal(bool bDestroyed);
    void UpdateBarrierState();

    bool IsSwordsman(const AEnemyAI* Enemy) const;
};
