
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

    void CheckWallAhead();

    bool IsValidWall(AActor* Actor) const;

    void PlayAttackAnimation();

    void StopAttackAnimation();

    // ======================================
    // TIMERS
    // ======================================

    FTimerHandle WallCheckTimer;
    FTimerHandle WallAttackTimer;

    FTimerHandle CapitolCheckTimer;
    FTimerHandle CapitolAttackTimer;

    // ======================================
    // STATES
    // ======================================

    bool bAttackingWall = false;
    bool bAttackingCapitol = false;
};
