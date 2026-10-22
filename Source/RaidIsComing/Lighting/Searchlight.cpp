#include "Searchlight.h"

#include "DayNightManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"

ASearchlight::ASearchlight()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    TowerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
    TowerMesh->SetupAttachment(SceneRoot);

    SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
    SpotLight->SetupAttachment(SceneRoot);
    SpotLight->SetMobility(EComponentMobility::Movable);
    SpotLight->SetRelativeLocation(FVector(0.0f, 0.0f, 400.0f));
    SpotLight->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));
    SpotLight->SetVisibility(false);
}

void ASearchlight::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplySettings();
}

void ASearchlight::BeginPlay()
{
    Super::BeginPlay();
    ApplySettings();

    if (IsValid(DayNightManager))
    {
        DayNightManager->OnNightStateChanged.AddUniqueDynamic(this, &ASearchlight::HandleNightStateChanged);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Searchlight %s: assign DayNightManager on the level instance."), *GetName());
    }
    RefreshActivation();
}

void ASearchlight::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(DayNightManager))
    {
        DayNightManager->OnNightStateChanged.RemoveDynamic(this, &ASearchlight::HandleNightStateChanged);
    }
    Super::EndPlay(EndPlayReason);
}

void ASearchlight::ApplySettings()
{
    SpotLight->SetIntensity(FMath::Max(0.0f, LightIntensity));
    SpotLight->SetLightColor(LightColor);
    const float Angle = FMath::Clamp(ConeHalfAngle, 1.0f, 80.0f);
    SpotLight->SetInnerConeAngle(Angle * 0.7f);
    SpotLight->SetOuterConeAngle(Angle);
    SpotLight->SetAttenuationRadius(FMath::Max(1.0f, MaxRange));
}

bool ASearchlight::IsLightActive() const
{
    return bActivationRequested && IsValid(DayNightManager)
        && DayNightManager->IsNight() && IsValid(SpotLight)
        && SpotLight->IsVisible() && SpotLight->Intensity > 0.0f;
}

void ASearchlight::RefreshActivation()
{
    const bool bEnable = bActivationRequested && IsValid(DayNightManager)
        && DayNightManager->IsNight() && LightIntensity > 0.0f;
    SpotLight->SetVisibility(bEnable);
    SetActorTickEnabled(bEnable);
}

void ASearchlight::SetActivationRequested(bool bRequested)
{
    bActivationRequested = bRequested;
    RefreshActivation();
}

void ASearchlight::HandleNightStateChanged(bool bNight)
{
    RefreshActivation();
}

void ASearchlight::SetAimLocation(FVector WorldLocation)
{
    if (!WorldLocation.ContainsNaN())
    {
        AimLocation = WorldLocation;
        bHasAimLocation = true;
    }
}

void ASearchlight::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLightActive())
    {
        RefreshActivation();
        return;
    }
    if (!bHasAimLocation)
    {
        return;
    }
    const FVector Direction = AimLocation - SpotLight->GetComponentLocation();
    if (!Direction.IsNearlyZero())
    {
        const FRotator Rotation = FMath::RInterpConstantTo(
            SpotLight->GetComponentRotation(), Direction.Rotation(),
            DeltaSeconds, FMath::Max(1.0f, RotationSpeed));
        SpotLight->SetWorldRotation(Rotation);
    }
}
