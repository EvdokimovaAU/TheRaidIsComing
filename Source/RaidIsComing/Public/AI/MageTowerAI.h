
#pragma once

#include "CoreMinimal.h"
#include "PlayerAI.h"
#include "TimerManager.h"
#include "MageTowerAI.generated.h"

class AMageProjectile;
class USceneComponent;
class ADayNightManager;

UCLASS()
class RAIDISCOMING_API AMageTowerAI : public APlayerAI
{
    GENERATED_BODY()

public:
    AMageTowerAI();

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "AI|Magic",
        meta = (ClampMin = "0.0"))
    float SplashRadius = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "AI|Magic",
        meta = (ClampMin = "0.01"))
    float TargetCheckInterval = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        Category = "Mage|Projectile")
    TSubclassOf<AMageProjectile> ProjectileClass;

    UPROPERTY(Transient, BlueprintReadOnly,
        Category = "Mage|Projectile")
    TObjectPtr<USceneComponent> ProjectileSpawnPoint;

    UFUNCTION(BlueprintCallable, Category = "Mage|Projectile")
    void SetProjectileSpawnPoint(USceneComponent* NewSpawnPoint);

    // Visual building actor to test for illumination.
    // Falls back to this actor when not assigned.
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite,
        Category = "AI|Lighting")
    TObjectPtr<AActor> IlluminationActor;

    // Automatically discovered if not assigned.
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
