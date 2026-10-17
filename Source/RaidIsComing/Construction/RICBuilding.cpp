#include "RICBuilding.h"

#include "Components/StaticMeshComponent.h"

ARICBuilding::ARICBuilding()
{
    PrimaryActorTick.bCanEverTick = false;

    BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("BuildingMesh")
    );

    SetRootComponent(BuildingMesh);

    BuildingMesh->SetCollisionProfileName(TEXT("BlockAll"));
    BuildingMesh->SetSimulatePhysics(false);

    SetCanBeDamaged(false);
}

void ARICBuilding::BeginPlay()
{
    Super::BeginPlay();

    BuildingId = FGuid::NewGuid();

    const bool bIsBarrier =
        BuildingType == ERICBuildingType::Barrier;

    SetCanBeDamaged(bIsBarrier);

    if (bIsBarrier)
    {
        if (!FMath::IsFinite(MaxHP) || MaxHP < 1.0f)
        {
            MaxHP = 100.0f;
        }

        CurrentHP = MaxHP;
    }
    else
    {
        CurrentHP = 0.0f;
    }

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Building initialized: %s. ID: %s. Destructible: %s. HP: %.1f"),
        *GetName(),
        *BuildingId.ToString(),
        bIsBarrier ? TEXT("Yes") : TEXT("No"),
        CurrentHP
    );
}

float ARICBuilding::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	// Башни и другие типы построек урон не получают.
	if (BuildingType != ERICBuildingType::Barrier)
	{
		return 0.0f;
	}

	if (!CanBeDamaged() ||
		IsActorBeingDestroyed() ||
		CurrentHP <= 0.0f ||
		!FMath::IsFinite(DamageAmount) ||
		DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage =
		FMath::Min(DamageAmount, CurrentHP);

	CurrentHP = FMath::Max(0.0f, CurrentHP - AppliedDamage);

	Super::TakeDamage(
		AppliedDamage,
		DamageEvent,
		EventInstigator,
		DamageCauser
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Barrier damaged: %s. Damage: %.1f. HP: %.1f"),
		*GetName(),
		AppliedDamage,
		CurrentHP
	);

	if (CurrentHP <= 0.0f)
	{
		Destroy();
	}

	return AppliedDamage;
}