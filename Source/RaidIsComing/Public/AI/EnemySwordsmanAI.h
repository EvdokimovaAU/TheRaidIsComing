
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

    // Existing wall combat
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallAttackCooldown = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float WallDamage = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Target")
    TObjectPtr<AActor> TargetWall = nullptr;

    virtual void OnObstacleDetected(AActor* Obstacle) override;

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StartWallAttack(AActor* Wall);

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void AttackWall();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StopWallAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void OnWallDestroyed();

    // Capitol combat
    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void CheckCapitolDistance();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StartCapitolAttack();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void AttackCapitol();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void StopCapitolAttack();

    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    FTimerHandle WallAttackTimer;
    FTimerHandle CapitolCheckTimer;
    FTimerHandle CapitolAttackTimer;

    bool bAttackingCapitol = false;
};
