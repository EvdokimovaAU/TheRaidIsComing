#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIBase.generated.h"

UCLASS()
class RAIDISCOMING_API AAIBase : public ACharacter
{
    GENERATED_BODY()

public:

    AAIBase();

    // Основные характеристики

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Stats")
    int32 ID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Stats")
    float MaxHP = 100.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Stats")
    float CurrentHP = 100.0f;

    // Характеристики атаки

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float Damage = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float AttackRange = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float AttackCooldown = 1.0f;

    // Текущая цель

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Combat")
    TObjectPtr<AAIBase> Target = nullptr;

    // Состояние

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
    bool bIsDead = false;

    // Методы

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    void ReceiveDamage(float Amount);

    UFUNCTION(BlueprintPure, Category = "AI|State")
    bool IsAlive() const;

    UFUNCTION(BlueprintPure, Category = "AI|Combat")
    bool IsTargetValid() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    bool CanAttack() const;

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    virtual void Attack();

    UFUNCTION(BlueprintCallable, Category = "AI|Combat")
    virtual void Die();

protected:

    virtual void BeginPlay() override;

    float LastAttackTime = -1000000.0f;
};
