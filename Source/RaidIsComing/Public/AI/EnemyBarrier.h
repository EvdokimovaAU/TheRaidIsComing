
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBarrier.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class RAIDISCOMING_API AEnemyBarrier : public AActor
{
    GENERATED_BODY()

public:
    AEnemyBarrier();

    UFUNCTION(BlueprintCallable, Category = "Barrier")
    void ActivateBarrier(float InHP, float InDuration);

    // ¬озвращает урон, который не поглотил барьер.
    UFUNCTION(BlueprintCallable, Category = "Barrier")
    float AbsorbDamage(float DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Barrier")
    void RemoveBarrier();

    UFUNCTION(BlueprintCallable, Category = "Barrier")
    void AttachToMage(AActor* Mage);

    UFUNCTION(BlueprintPure, Category = "Barrier")
    bool IsBarrierActive() const;

    UFUNCTION(BlueprintPure, Category = "Barrier")
    bool ContainsLocation(FVector Location) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Barrier",
        meta = (ClampMin = "1.0"))
    float BarrierRadius = 180.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Barrier")
    float CurrentBarrierHP = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Barrier|Visual")
    TObjectPtr<UMaterialInterface> BarrierMaterial;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Barrier")
    TObjectPtr<USphereComponent> SphereCollision;

    UPROPERTY(VisibleAnywhere, Category = "Barrier")
    TObjectPtr<UStaticMeshComponent> SphereVisual;

    FTimerHandle ExpirationTimer;

    bool bActive = false;

    void UpdateBarrierSize();
};
