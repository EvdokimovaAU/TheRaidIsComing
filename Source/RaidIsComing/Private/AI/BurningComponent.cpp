
#include "AI/BurningComponent.h"

#include "Construction/RICBuilding.h"

#include "NiagaraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"

#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"

UBurningComponent::UBurningComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UBurningComponent::Ignite(AActor* InstigatorActor)
{
    if (bBurning || !GetWorld())
        return false;

    ARICBuilding* Building = Cast<ARICBuilding>(GetOwner());

    if (!IsValid(Building) ||
        !Building->IsAlive() ||
        !Building->IsDestructible())
    {
        return false;
    }

    bBurning = true;
    FireInstigator = InstigatorActor;

    // Spawn Niagara fire on the building.
    if (FireEffect && Building->BuildingMesh && GetWorld()->GetNetMode() != NM_DedicatedServer)
    {
        const FBoxSphereBounds Bounds = Building->BuildingMesh->Bounds;
        const FVector FireBase = Bounds.Origin - FVector(0, 0, Bounds.BoxExtent.Z)
            + Building->GetActorRotation().RotateVector(FireOffset);
        ActiveFire = UNiagaraFunctionLibrary::
            SpawnSystemAttached(
                FireEffect,
                Building->BuildingMesh,
                NAME_None,
                FireBase,
                FRotator::ZeroRotator,
                EAttachLocation::KeepWorldPosition,
                false,
                false,
                ENCPoolMethod::None,
                false
            );

        if (ActiveFire)
        {
            ActiveFire->SetWorldScale3D(FireScale);
            ActiveFire->Activate();
        }
    }

    // Damage once per second.
    GetWorld()->GetTimerManager().SetTimer(
        DamageTimer,
        this,
        &UBurningComponent::ApplyBurnDamage,
        1.0f,
        true
    );

    // Fire ends after configured duration.
    GetWorld()->GetTimerManager().SetTimer(
        DurationTimer,
        this,
        &UBurningComponent::Extinguish,
        FMath::Max(BurnDuration, 0.1f),
        false
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[Burning] %s ignited"),
        *Building->GetName());

    return true;
}

void UBurningComponent::ApplyBurnDamage()
{
    ARICBuilding* Building = Cast<ARICBuilding>(GetOwner());

    if (!IsValid(Building) || !Building->IsAlive())
    {
        Extinguish();
        return;
    }

    FDamageEvent DamageEvent(UDamageType::StaticClass());

    AActor* Causer = FireInstigator.Get();

    AController* InstigatorController =
        IsValid(Causer) ? Causer->GetInstigatorController() : nullptr;

    Building->TakeDamage(
        FMath::Max(0.f, DamagePerSecond),
        DamageEvent,
        InstigatorController,
        IsValid(Causer) ? Causer : Building
    );

    if (!Building->IsAlive())
    {
        Extinguish();
    }
}

void UBurningComponent::Extinguish()
{
    if (!bBurning)
        return;

    bBurning = false;

    GetWorld()->GetTimerManager().ClearTimer(DamageTimer);
    GetWorld()->GetTimerManager().ClearTimer(DurationTimer);

    if (IsValid(ActiveFire))
    {
        ActiveFire->Deactivate();
        ActiveFire->DestroyComponent();
    }

    ActiveFire = nullptr;
    FireInstigator.Reset();

    UE_LOG(LogTemp, Log,
        TEXT("[Burning] Fire extinguished on %s"),
        *GetNameSafe(GetOwner()));
}

void UBurningComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    Extinguish();
    Super::EndPlay(EndPlayReason);
}
