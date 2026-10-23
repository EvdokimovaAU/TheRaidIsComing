
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"
#include "RICBuildTypes.h"
#include "RICBuilding.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class RAIDISCOMING_API ARICBuilding : public AActor
{
    GENERATED_BODY()

public:

    ARICBuilding();

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    UFUNCTION(BlueprintPure, Category = "Building|Health")
    bool IsAlive() const;

    UFUNCTION(BlueprintPure, Category = "Building|Health")
    bool IsDestructible() const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
    TObjectPtr<UStaticMeshComponent> BuildingMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    ERICBuildingType BuildingType = ERICBuildingType::Barracks;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Construction",
        meta = (ClampMin = "0")
    )
    int32 ConstructionCost = 100;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Building"
    )
    FGuid BuildingId;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Building|Health",
        meta = (ClampMin = "1.0")
    )
    float MaxHP = 100.0f;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Building|Health"
    )
    float CurrentHP = 0.0f;

protected:

    virtual void BeginPlay() override;
};
