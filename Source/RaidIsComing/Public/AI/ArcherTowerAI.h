
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "ArcherTowerAI.generated.h"

class AProjectileArcher;
class USceneComponent;
class ADayNightManager;

UCLASS()
class RAIDISCOMING_API AArcherTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:
    AArcherTowerAI();

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "AI|Archer",
        meta = (ClampMin = "0.01"))
    float TargetCheckInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Archer|Projectile")
    TSubclassOf<AProjectileArcher> ProjectileClass;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Archer|Projectile")
    TObjectPtr<USceneComponent> ProjectileSpawnPoint;

    UFUNCTION(BlueprintCallable, Category = "Archer|Projectile")
    void SetProjectileSpawnPoint(USceneComponent* NewSpawnPoint);

    // Building to check against the searchlight.
    // If empty, checks this AI actor.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite,
        Category = "AI|Lighting")
    TObjectPtr<AActor> IlluminationActor;

    // Optional; automatically discovered at BeginPlay.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite,
        Category = "AI|Lighting")
    TObjectPtr<ADayNightManager> DayNightManager;

    UFUNCTION(BlueprintPure, Category = "AI|Lighting")
    bool CanFireUnderLighting() const;

    virtual void Attack() override;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

private:
    FTimerHandle TargetCheckTimer;
    float NextShotTime = 0.0f;

    void UpdateTower();
};
