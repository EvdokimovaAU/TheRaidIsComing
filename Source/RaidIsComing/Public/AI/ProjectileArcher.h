
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
    // Башня передаёт цель и количество урона.
    void InitializeProjectile(
        AAIBase* NewTarget,
        float NewDamage
    );

    // Скорость полёта стрелы (Unreal Units / сек).
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "1.0"))
    float FlightSpeed = 900.0f;

    // Максимальная высота дуги.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Movement",
        meta = (ClampMin = "0.0"))
    float ArcHeight = 120.0f;

    // Радиус, в пределах которого засчитывается попадание.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Projectile|Combat",
        meta = (ClampMin = "1.0"))
    float HitRadius = 35.0f;

protected:

    virtual void BeginPlay() override;

    // Корневой компонент стрелы.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly,
        Category = "Projectile")
    TObjectPtr<USceneComponent> SceneRoot;

private:

    // Цель, в которую летит стрела.
    TWeakObjectPtr<AAIBase> TargetActor;

    // Урон, переданный башней.
    float ProjectileDamage = 0.0f;

    // Начальная позиция стрелы.
    FVector StartLocation = FVector::ZeroVector;

    // Полная продолжительность полёта.
    float FlightDuration = 1.0f;

    // Время, прошедшее с момента запуска.
    float ElapsedTime = 0.0f;

    // Была ли стрела инициализирована.
    bool bInitialized = false;

    // Обработка попадания по врагу.
    void HitTarget();
};
