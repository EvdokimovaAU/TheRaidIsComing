
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "ArcherTowerAI.generated.h"

class AProjectileArcher;
class USceneComponent;

UCLASS()
class RAIDISCOMING_API AArcherTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:

    AArcherTowerAI();

    // Интервал проверки противников
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "AI|Archer",
        meta = (ClampMin = "0.01"))
    float TargetCheckInterval = 0.2f;

    // Blueprint стрелы, которую создаёт башня
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Archer|Projectile")
    TSubclassOf<AProjectileArcher> ProjectileClass;

    // Точка появления стрелы
    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Archer|Projectile")
    TObjectPtr<USceneComponent> ProjectileSpawnPoint;

    // Передача точки выстрела из BP_ArcherTower
    UFUNCTION(BlueprintCallable,
        Category = "Archer|Projectile")
    void SetProjectileSpawnPoint(
        USceneComponent* NewSpawnPoint
    );

    // Выполнение атаки
    virtual void Attack() override;

protected:

    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:

    // Таймер проверки противников
    FTimerHandle TargetCheckTimer;

    // Время, когда разрешён следующий выстрел
    float NextShotTime = 0.0f;

    // Основной цикл башни
    void UpdateTower();
};
