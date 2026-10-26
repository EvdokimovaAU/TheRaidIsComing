
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Wall|Decision")
    float WallAvoidanceDistance = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Wall|Decision")
    float WallAvoidanceTimeout = 5.0f;

    // Local movement settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Wall|Approach")
    float WallApproachSpeed = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Wall|Approach")
    float WallApproachTimeout = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Wall|Approach")
    float WallAttackDistance = 115.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "AI|Wall")
    TObjectPtr<ARICBuilding> TargetWall = nullptr;

    virtual void Tick(float DeltaSeconds) override;

    virtual void OnObstacleDetected(AActor* Obstacle) override;

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StartWallAttack(AActor* Wall);

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void AttackWall();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void StopWallAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Wall")
    void OnWallDestroyed();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void CheckCapitolDistance();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StartCapitolAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void AttackCapitol();

    UFUNCTION(BlueprintCallable, Category = "AI|Capitol")
    void StopCapitolAttack();

    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
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

    TWeakObjectPtr<ARICBuilding> AvoidedWall;

    FVector AvoidanceDestination = FVector::ZeroVector;

    bool bAvoidingWall = false;
    bool bAttackingWall = false;
    bool bApproachingWall = false;
    bool bAttackingCapitol = false;

    float AvoidanceStartTime = 0.0f;
    float ApproachStartTime = 0.0f;
    float PreviousWallDistance = 0.0f;
    float StuckTime = 0.0f;

    FTimerHandle WallCheckTimer;
    FTimerHandle WallAttackTimer;
    FTimerHandle WallAvoidanceTimer;
    FTimerHandle CapitolCheckTimer;
    FTimerHandle CapitolAttackTimer;
};
