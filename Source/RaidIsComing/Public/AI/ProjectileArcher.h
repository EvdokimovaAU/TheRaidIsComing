
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProjectileArcher.generated.h"

class AAIBase;
class USceneComponent;

UCLASS()
class RAIDISCOMING_API AProjectileArcher : public AActor
{
    GENERATED_BODY()

public:
    AProjectileArcher();

    virtual void Tick(float DeltaTime) override;

    // Настройка стрелы перед запуском.
    void InitializeProjectile(
        AAIBase* NewTarget,
        float NewDamage
    );

    // Скорость полёта стрелы.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "1.0"))
    float FlightSpeed = 900.0f;

    // Максимальная высота дуги.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "0.0"))
    float ArcHeight = 120.0f;

    // Радиус попадания.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Combat",
        meta = (ClampMin = "1.0"))
    float HitRadius = 35.0f;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Projectile")
    TObjectPtr<USceneComponent> SceneRoot;

private:
    TWeakObjectPtr<AAIBase> TargetActor;

    float ProjectileDamage = 0.0f;

    FVector StartLocation = FVector::ZeroVector;

    float FlightDuration = 1.0f;

    float ElapsedTime = 0.0f;

    bool bInitialized = false;

    // Обработка попадания по врагу или барьеру.
    void HitTarget();
};
