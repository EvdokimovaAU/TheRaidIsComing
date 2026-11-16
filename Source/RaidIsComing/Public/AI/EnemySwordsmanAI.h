
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "EnemySwordsmanAI.generated.h"

class ARICBuilding;
class ANavigationData;

UCLASS()
class RAIDISCOMING_API AEnemySwordsmanAI : public AEnemyAI
{
    GENERATED_BODY()

public:

    AEnemySwordsmanAI();

    // WALL SETTINGS

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallAttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallDetectionDistance = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallCheckInterval = 0.2f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Wall|Decision",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float WallAttackChance = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Wall|Decision")
    float WallAvoidanceDistance = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Wall|Decision")
    float WallAvoidanceTimeout = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Wall|Approach")
    float WallApproachSpeed = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Wall|Approach")
    float WallApproachTimeout = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Wall|Approach")
    float WallAttackDistance = 115.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    TObjectPtr<ARICBuilding> TargetWall = nullptr;

    // INITIAL MOVEMENT

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Mage")
    float InitialMovementDelay = 0.8f;

    bool bInitialMovementAllowed = false;

    FTimerHandle InitialMovementTimer;

    void AllowInitialMovement();

    // BASE AI

    virtual void Tick(float DeltaSeconds) override;

    virtual void OnObstacleDetected(AActor* Obstacle) override;

    virtual void MoveToTarget() override;

    virtual void ApplyEnemyBalance() override;

    virtual void Die() override;

    // WALL COMBAT

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StartWallAttack(AActor* Wall);

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void AttackWall();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StopWallAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void OnWallDestroyed();

    // CAPITOL COMBAT

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void CheckCapitolDistance();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StartCapitolAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void AttackCapitol();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StopCapitolAttack();

    // MAGE ESCORT

    UFUNCTION(BlueprintCallable, Category = "AI|Mage")
    void WaitForMage(AActor* Mage);

    UFUNCTION(BlueprintCallable, Category = "AI|Mage")
    void ResumeAfterMage(AActor* Mage);

    bool TryAssignEscortMage(AActor* Mage);

    void ReleaseEscortMage(AActor* Mage);

    bool IsAssignedToMage(const AActor* Mage) const;

    bool HasEscortMage() const;

    bool IsWaitingForMage() const;

    bool IsAttackingWall() const;

    // The escort mage authorizes damage only after the entire group is in range.
    void AuthorizeEscortWallAttack(AActor* Mage, AActor* Wall);
    float GetEscortWallDistance(const AActor* Wall) const;
    bool IsEscortWallAttackAuthorized() const;


    bool IsAttackingCapitol() const;

    // ESCORT SPEED CONTROL

    // Called by the mage to smoothly adjust
    // swordsman movement speed.
    void SetEscortSpeedLimit(
        AActor* RequestingMage,
        float TargetSpeed,
        float DeltaTime
    );

    // Restores the original movement speed
    // when escort control ends.
    void ClearEscortSpeedLimit(
        AActor* RequestingMage
    );

protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:

    friend class AEnemyMageAI;
    friend class FEnemyEscortObstacleTest;
    friend class FEnemyEscortMarchTest;
    UFUNCTION()
    void OnCombatNavigationChanged(ANavigationData* NavData);
    FVector RejectedFromPosition = FVector::ZeroVector;
    mutable float NextCapitolRouteCheck = 0.f;
    mutable bool bCachedCapitolRoute = false;
    bool IsEscortMovementControlled() const;
    bool IsEscortCombatBlocked() const;
    TMap<TWeakObjectPtr<ARICBuilding>, float> RejectedWalls;
    FVector WallApproachDestination = FVector::ZeroVector;
    bool bWallApproachMoveIssued = false;

    // WALL HELPERS

    void CheckWallAhead();

    bool IsValidWall(AActor* Actor) const;

    float GetDistanceToWall(const AActor* Wall) const;

    FVector GetClosestWallPoint(const AActor* Wall) const;

    bool HasFreePathToCapitol() const;

    bool TryAvoidWall(AActor* Wall);

    void UpdateWallAvoidance();

    void StopWallAvoidance();

    void UpdateWallApproach(float DeltaSeconds);

    void BeginWallDamage();

    void StopWallApproach();

    void PlayAttackAnimation();

    void StopAttackAnimation();

    // MAGE HELPERS

    TWeakObjectPtr<AActor> EscortMage;

    TWeakObjectPtr<AActor> WaitingForMage;

    void CheckMageWaitingState();

    // ESCORT SPEED STATE

    // Mage currently controlling the speed.
    TWeakObjectPtr<AActor> SpeedLimitOwner;

    // Original speed before escort adjustment.
    float OriginalWalkSpeed = 0.0f;

    // WALL STATE

    TWeakObjectPtr<ARICBuilding> AvoidedWall;

    FVector AvoidanceDestination = FVector::ZeroVector;

    bool bAvoidingWall = false;

    bool bAttackingWall = false;

    bool bEscortWallAttackAuthorized = false;

    bool bApproachingWall = false;

    bool bAttackingCapitol = false;

    float AvoidanceStartTime = 0.0f;

    float ApproachStartTime = 0.0f;

    float PreviousWallDistance = 0.0f;

    float StuckTime = 0.0f;

    float NextWallNavRetryTime = 0.f;

    float SavedWalkSpeed = 0.0f;

    // TIMERS

    FTimerHandle WallCheckTimer;

    FTimerHandle WallAttackTimer;

    FTimerHandle WallAvoidanceTimer;

    FTimerHandle CapitolCheckTimer;

    FTimerHandle CapitolAttackTimer;

    FTimerHandle MageWaitingTimer;
};
