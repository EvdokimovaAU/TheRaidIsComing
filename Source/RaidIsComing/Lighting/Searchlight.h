#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Searchlight.generated.h"

class ADayNightManager;
class USceneComponent;
class UStaticMeshComponent;
class USpotLightComponent;

UCLASS(Blueprintable)
class RAIDISCOMING_API ASearchlight : public AActor
{
    GENERATED_BODY()

public:
    ASearchlight();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    // Внешний контроллер передаёт состояние Shift и точку попадания курсора.
    UFUNCTION(BlueprintCallable, Category = "Searchlight")
    void SetActivationRequested(bool bRequested);

    UFUNCTION(BlueprintCallable, Category = "Searchlight")
    void SetAimLocation(FVector WorldLocation);

    UFUNCTION(BlueprintPure, Category = "Searchlight")
    bool IsLightActive() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Searchlight")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Searchlight")
    TObjectPtr<UStaticMeshComponent> TowerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Searchlight")
    TObjectPtr<USpotLightComponent> SpotLight;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Searchlight")
    TObjectPtr<ADayNightManager> DayNightManager;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Searchlight|Settings", meta = (ClampMin = "0"))
    float LightIntensity = 100000.0f;

    // Половина внешнего угла конуса, в градусах.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Searchlight|Settings", meta = (ClampMin = "1", ClampMax = "80"))
    float ConeHalfAngle = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Searchlight|Settings", meta = (ClampMin = "1", Units = "cm"))
    float MaxRange = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Searchlight|Settings", meta = (ClampMin = "1"))
    float RotationSpeed = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Searchlight|Settings")
    FLinearColor LightColor = FLinearColor::White;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool bActivationRequested = false;
    bool bHasAimLocation = false;
    FVector AimLocation = FVector::ZeroVector;

    UFUNCTION()
    void HandleNightStateChanged(bool bNight);

    void ApplySettings();
    void RefreshActivation();
};
