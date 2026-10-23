
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemySwordsmanAnimInstance.generated.h"

UCLASS()
class RAIDISCOMING_API UEnemySwordsmanAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:

    virtual void NativeInitializeAnimation() override;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // Current movement speed
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
    float Speed = 0.0f;

    // Is the enemy moving?
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
    bool bIsMoving = false;

    // Is the enemy attacking?
    UPROPERTY(BlueprintReadOnly, Category = "Animation|Combat")
    bool bIsAttacking = false;

    // Attack animation asset
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Combat")
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

    // Start attack animation
    UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
    void PlayAttackAnimation();

    // Stop attack animation
    UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
    void StopAttackAnimation();

    // Update attack state
    UFUNCTION(BlueprintCallable, Category = "Animation|Combat")
    void SetAttacking(bool bNewAttacking);

private:

    UPROPERTY(Transient)
    TObjectPtr<APawn> OwnerPawn = nullptr;
};
