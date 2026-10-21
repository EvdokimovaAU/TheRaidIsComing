
#pragma once

#include "CoreMinimal.h"
#include "AI/AIBase.h"
#include "TimerManager.h"
#include "EnemyAI.generated.h"

UCLASS()
class RAIDISCOMING_API AEnemyAI : public AAIBase
{
    GENERATED_BODY()

public:

    AEnemyAI();

    // Œ—ÕŒ¬Õ¿ﬂ ÷≈À‹

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Target")
    TObjectPtr<AActor> Capitol = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "AI|Target")
    TObjectPtr<AActor> CurrentTarget = nullptr;


    // Õ¿¬»√¿÷»ﬂ

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "AI|Navigation")
    float RouteProgress = 0.0f;


    // Õ¿—“–Œ… » œ≈–≈Ã≈Ÿ≈Õ»ﬂ

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Movement",
        meta = (ClampMin = "1.0"))
    float AcceptanceRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Movement",
        meta = (ClampMin = "100.0"))
    float NavigationSearchRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Movement")
    float ObstacleDetectionRadius = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Movement")
    float MovementSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "AI|Movement")
    bool bAutoStartMovement = true;


    // ”œ–¿¬À≈Õ»≈ ÷≈À‹ﬁ

    UFUNCTION(BlueprintCallable,
        Category = "AI|Target")
    void SetCapitol(AActor* NewCapitol);

    UFUNCTION(BlueprintCallable,
        Category = "AI|Target")
    AActor* GetCurrentTarget() const;


    // ”œ–¿¬À≈Õ»≈ œ≈–≈Ã≈Ÿ≈Õ»≈Ã

    UFUNCTION(BlueprintCallable,
        Category = "AI|Movement")
    virtual void MoveToTarget();

    UFUNCTION(BlueprintCallable,
        Category = "AI|Movement")
    virtual void StopMovement();

    UFUNCTION(BlueprintCallable,
        Category = "AI|Movement")
    virtual void OnObstacleDetected(AActor* Obstacle);


    // —Ã≈–“‹

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
