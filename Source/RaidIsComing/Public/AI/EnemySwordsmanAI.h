
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "EnemySwordsmanAI.generated.h"

class ARICBuilding;

UCLASS()
class RAIDISCOMING_API AEnemySwordsmanAI : public AEnemyAI
{
    GENERATED_BODY()

public:

    AEnemySwordsmanAI();

    // ======================================
    // WALL SETTINGS
    // ======================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallAttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallDamage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallDetectionDistance = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    float WallCheckInterval = 0.2f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Wall")
    TObjectPtr<ARICBuilding> TargetWall = nullptr;

    // ======================================
    // WALL DECISION SETTINGS
    // ======================================

    // 50 = 50% attack, 50% avoid
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Wall|Decision",
        meta = (ClampMin = "0.0", ClampMax = "100.0")
    )
    float WallAttackChance = 50.0f;

    // Distance used to search for a waypoint
    // beside the obstacle
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Wall|Decision",
        meta = (ClampMin = "50.0")
    )
    float WallAvoidanceDistance = 350.0f;

    // Maximum duration of an avoidance attempt
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "AI|Wall|Decision",
        meta = (ClampMin = "0.5")
    )
    float WallAvoidanceTimeout = 5.0f;

    // ======================================
    // WALL COMBAT
    // ======================================

    virtual void OnObstacleDetected(AActor* Obstacle) override;

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StartWallAttack(AActor* Wall);

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void AttackWall();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StopWallAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void OnWallDestroyed();

    // ======================================
    // CAPITOL COMBAT
    // ======================================

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void CheckCapitolDistance();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StartCapitolAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void AttackCapitol();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StopCapitolAttack();

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

    // ======================================
    // WALL DETECTION
    // ======================================

    void CheckWallAhead();

    bool IsValidWall(AActor* Actor) const;

    // ======================================
    // WALL AVOIDANCE
    // ======================================

    bool TryAvoidWall(AActor* Wall);

    void UpdateWallAvoidance();

    void StopWallAvoidance();

    TWeakObjectPtr<ARICBuilding> AvoidedWall;

    FVector AvoidanceDestination = FVector::ZeroVector;

    bool bAvoidingWall = false;

    float AvoidanceStartTime = 0.0f;

    // ======================================
    // ANIMATIONS
    // ======================================

    void PlayAttackAnimation();

    void StopAttackAnimation();

    // ======================================
    // TIMERS
    // ======================================

    FTimerHandle WallCheckTimer;
    FTimerHandle WallAttackTimer;
    FTimerHandle WallAvoidanceTimer;

    FTimerHandle CapitolCheckTimer;
    FTimerHandle CapitolAttackTimer;

    // ======================================
    // STATES
    // ======================================

    bool bAttackingWall = false;
    bool bAttackingCapitol = false;
};
