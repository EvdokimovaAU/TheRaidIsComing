
#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyAI.h"
#include "TimerManager.h"
#include "EnemyMageAI.generated.h"

UCLASS()
class RAIDISCOMING_API AEnemyMageAI : public AEnemyAI
{
    GENERATED_BODY()

public:
    AEnemyMageAI();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float AllyDetectionRadius = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierCooldown = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    float BarrierDuration = 10.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Magic")
    TArray<TObjectPtr<AEnemyAI>> AlliesAhead;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void FindAlliesAhead();

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    bool CanCreateBarrier() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Magic")
    void Barrier();

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle BarrierTimer;

    void UpdateBarrier();
};
