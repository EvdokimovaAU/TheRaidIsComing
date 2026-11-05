
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BurningComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAIDISCOMING_API UBurningComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBurningComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burning")
    float BurnDuration = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burning")
    float DamagePerSecond = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burning")
    TObjectPtr<UNiagaraSystem> FireEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burning")
    FVector FireOffset = FVector(0, 0, 100);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burning")
    FVector FireScale = FVector(1, 1, 1);

    UFUNCTION(BlueprintCallable, Category = "Burning")
    bool Ignite(AActor* InstigatorActor);

    UFUNCTION(BlueprintPure, Category = "Burning")
    bool IsBurning() const { return bBurning; }

    UFUNCTION(BlueprintCallable, Category = "Burning")
    void Extinguish();

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    bool bBurning = false;

    TWeakObjectPtr<AActor> FireInstigator;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> ActiveFire = nullptr;

    FTimerHandle DamageTimer;
    FTimerHandle DurationTimer;

    void ApplyBurnDamage();
};
