
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MageProjectile.generated.h"

class AAIBase;
class USceneComponent;

UCLASS()
class RAIDISCOMING_API AMageProjectile : public AActor
{
    GENERATED_BODY()

public:

    AMageProjectile();

    virtual void Tick(float DeltaTime) override;

    // Инициализация снаряда после создания башней
    void InitializeProjectile(
        AAIBase* NewTarget,
        float NewDamage,
        float NewSplashRadius
    );

    // Скорость полёта магического шара
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "1.0"))
    float FlightSpeed = 450.0f;

    // Расстояние, на котором засчитывается попадание
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "1.0"))
    float HitRadius = 40.0f;

protected:

    virtual void BeginPlay() override;

    // Корневой компонент для визуала в Blueprint
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Projectile")
    TObjectPtr<USceneComponent> SceneRoot;

private:

    // Противник, которого преследует магический шар
    TWeakObjectPtr<AAIBase> TargetActor;

    // Урон каждому врагу в радиусе взрыва
    float ProjectileDamage = 0.0f;

    // Радиус массового урона
    float ProjectileSplashRadius = 0.0f;

    // Получены ли данные от башни
    bool bInitialized = false;

    // Защита от повторного попадания
    bool bHasExploded = false;

    // Обработка попадания
    void HitTarget();

    // Массовый урон по противникам
    void ApplySplashDamage(const FVector& ImpactLocation);
};
