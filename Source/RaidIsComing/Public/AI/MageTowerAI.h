
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "MageTowerAI.generated.h"

class AMageProjectile;
class USceneComponent;

UCLASS()
class RAIDISCOMING_API AMageTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:

    AMageTowerAI();

    // Радиус массового урона при попадании магии
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic",
        meta = (ClampMin = "0.0"))
    float SplashRadius = 200.0f;

    // Частота проверки противников
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Magic",
        meta = (ClampMin = "0.01"))
    float TargetCheckInterval = 0.2f;

    // Blueprint магического снаряда
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mage|Projectile")
    TSubclassOf<AMageProjectile> ProjectileClass;

    // Точка появления магии
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Mage|Projectile")
    TObjectPtr<USceneComponent> ProjectileSpawnPoint;

    // Получаем точку выстрела из BP_MageTower
    UFUNCTION(BlueprintCallable, Category = "Mage|Projectile")
    void SetProjectileSpawnPoint(USceneComponent* NewSpawnPoint);

    virtual void Attack() override;

protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:

    FTimerHandle TargetCheckTimer;

    // Время следующего разрешённого выстрела
    float NextShotTime = 0.0f;

    // Основной цикл башни
    void UpdateTower();
};
