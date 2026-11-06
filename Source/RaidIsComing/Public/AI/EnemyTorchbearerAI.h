
#pragma once
#include "AI/GameBalanceManager.h"
#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "TimerManager.h"
#include "EnemyTorchbearerAI.generated.h"

class ARICBuilding;
class UAnimMontage;
class UBurningComponent;

UCLASS()
class RAIDISCOMING_API AEnemyTorchbearerAI : public AEnemyAI
{
    GENERATED_BODY()

public:
    AEnemyTorchbearerAI();
    virtual void ApplyEnemyBalance() override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Movement",
        meta = (ClampMin = "0.05"))
    float CheckInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Movement",
        meta = (ClampMin = "1.0"))
    float FireRange = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Walls",
        meta = (ClampMin = "1.0"))
    float WallDetectionDistance = 180.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Fire",
        meta = (ClampMin = "0.01"))
    float IgniteDelay = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Fire",
        meta = (ClampMin = "0.0"))
    float ReigniteDelay = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Torchbearer|Animation")
    TObjectPtr<UAnimMontage> IgniteMontage = nullptr;

    virtual void MoveToTarget() override;
    virtual void Die() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle BehaviorTimer;
    FTimerHandle IgniteTimer;

    TWeakObjectPtr<ARICBuilding> FireTarget;

    bool bPreparingIgnition = false;
    bool bHasIgnitedTarget = false;

    float NextIgnitionAllowedTime = 0.f;

    void UpdateBehavior();

    ARICBuilding* FindWallAhead() const;

    UBurningComponent* GetBurningComponent(
        AActor* Actor
    ) const;

    float GetDistanceToBuilding(
        const ARICBuilding* Building
    ) const;

    void StartIgnition(ARICBuilding* Building);
    void CompleteIgnition();
    void CancelIgnition();

    void ResumeMovement();
};
