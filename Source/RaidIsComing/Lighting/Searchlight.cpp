#include "Searchlight.h"

#include "DayNightManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

namespace SearchlightGeometry
{
    // All coordinates are relative to the light and divided by its range.
    // The light volume is a circular cone intersected with a unit sphere:
    // attenuation distance is radial, not distance along the cone axis.
    struct FVolume
    {
        FVector Center = FVector::ZeroVector;
        FVector HalfAxes[3] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
        FVector CapsuleHalfSegment = FVector::ZeroVector;
        double Radius = 0.0;
    };

    FVector BoxSupport(const FVolume& Box, const FVector& Direction)
    {
        FVector Result = Box.Center;
        for (int32 I = 0; I < 3; ++I)
        {
            Result += Box.HalfAxes[I] *
                (FVector::DotProduct(Box.HalfAxes[I], Direction) >= 0.0 ? 1.0 : -1.0);
        }
        Result += Box.CapsuleHalfSegment *
            (FVector::DotProduct(Box.CapsuleHalfSegment, Direction) >= 0.0 ? 1.0 : -1.0);
        const double DirectionLength = Direction.Size();
        if (Box.Radius > 0.0 && DirectionLength > 1.e-15)
            Result += Direction * (Box.Radius / DirectionLength);
        return Result;
    }

    FVector LightSupport(const FVector& Direction, const FVector& Axis,
        double CosAngle, double SinAngle)
    {
        const double Length = Direction.Size();
        if (Length < 1.e-15) return FVector::ZeroVector;
        const double Axial = FVector::DotProduct(Direction, Axis);
        if (Axial >= Length * CosAngle) return Direction / Length;
        const FVector Radial = Direction - Axis * Axial;
        const double RadialLength = Radial.Size();
        if (RadialLength < 1.e-15) return FVector::ZeroVector;
        const FVector Rim = Axis * CosAngle + Radial * (SinAngle / RadialLength);
        return FVector::DotProduct(Rim, Direction) > 0.0 ? Rim : FVector::ZeroVector;
    }

    // Closest point to the origin on a simplex (up to 4 vertices).
    // Enumerate its faces and solve barycentric coordinates. Degenerate faces
    // fall back to their edges/vertices, including for flat object bounds.
    FVector ReduceSimplex(FVector* Points, int32& Count)
    {
        double BestDistance = 1.e300;
        FVector Best = Points[0];
        int32 BestIndices[4] = { 0, 0, 0, 0 };
        int32 BestCount = 1;
        for (int32 Mask = 1; Mask < (1 << Count); ++Mask)
        {
            int32 Indices[4];
            int32 N = 0;
            for (int32 I = 0; I < Count; ++I)
                if (Mask & (1 << I)) Indices[N++] = I;
            const FVector Base = Points[Indices[0]];
            FVector Edges[3];
            double Matrix[3][4] = {};
            const int32 Dim = N - 1;
            for (int32 I = 0; I < Dim; ++I) Edges[I] = Points[Indices[I + 1]] - Base;
            for (int32 I = 0; I < Dim; ++I)
            {
                for (int32 J = 0; J < Dim; ++J)
                    Matrix[I][J] = FVector::DotProduct(Edges[I], Edges[J]);
                Matrix[I][Dim] = -FVector::DotProduct(Edges[I], Base);
            }
            bool bValid = true;
            for (int32 Col = 0; Col < Dim; ++Col)
            {
                int32 Pivot = Col;
                for (int32 Row = Col + 1; Row < Dim; ++Row)
                    if (FMath::Abs(Matrix[Row][Col]) > FMath::Abs(Matrix[Pivot][Col])) Pivot = Row;
                if (FMath::Abs(Matrix[Pivot][Col]) < 1.e-20) { bValid = false; break; }
                for (int32 J = Col; J <= Dim; ++J) Swap(Matrix[Col][J], Matrix[Pivot][J]);
                const double Divisor = Matrix[Col][Col];
                for (int32 J = Col; J <= Dim; ++J) Matrix[Col][J] /= Divisor;
                for (int32 Row = 0; Row < Dim; ++Row)
                {
                    if (Row == Col) continue;
                    const double Factor = Matrix[Row][Col];
                    for (int32 J = Col; J <= Dim; ++J) Matrix[Row][J] -= Factor * Matrix[Col][J];
                }
            }
            if (!bValid) continue;
            double BaseWeight = 1.0;
            FVector Candidate = Base;
            for (int32 I = 0; I < Dim; ++I)
            {
                const double Weight = Matrix[I][Dim];
                if (Weight < 0.0) bValid = false;
                BaseWeight -= Weight;
                Candidate += Edges[I] * Weight;
            }
            if (!bValid || BaseWeight < 0.0) continue;
            const double Distance = Candidate.SizeSquared();
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                Best = Candidate;
                BestCount = N;
                for (int32 I = 0; I < N; ++I) BestIndices[I] = Indices[I];
            }
        }
        FVector Reduced[4];
        for (int32 I = 0; I < BestCount; ++I) Reduced[I] = Points[BestIndices[I]];
        Count = BestCount;
        for (int32 I = 0; I < Count; ++I) Points[I] = Reduced[I];
        return Best;
    }

    bool Intersects(const FVolume& Box, const FVector& Axis, double CosAngle,
        double SinAngle, double Tolerance)
    {
        const auto Support = [&](const FVector& Direction)
            {
                return BoxSupport(Box, Direction) - LightSupport(-Direction, Axis, CosAngle, SinAngle);
            };
        FVector Points[4];
        int32 Count = 1;
        Points[0] = Support(Axis);
        FVector Closest = Points[0];
        // GJK distance query; no corner sampling, so a cone crossing a face
        // is detected even when none of the box's corners are inside it.
        for (int32 Iteration = 0; Iteration < 128; ++Iteration)
        {
            const double Distance = Closest.Size();
            if (Distance <= Tolerance) return true;
            const FVector Direction = -Closest / Distance;
            const FVector Next = Support(Direction);
            // A separating plane proves the entire box is outside.
            if (FVector::DotProduct(Next, Direction) < -Tolerance) return false;
            bool bDuplicate = false;
            for (int32 I = 0; I < Count; ++I)
                if ((Next - Points[I]).SizeSquared() < 1.e-28) bDuplicate = true;
            if (bDuplicate || Count == 4) return false;
            Points[Count++] = Next;
            Closest = ReduceSimplex(Points, Count);
        }
        return Closest.SizeSquared() <= Tolerance * Tolerance;
    }
}

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


bool ASearchlight::IsActorIlluminated(const AActor* TargetActor) const
{
    if (!IsLightActive() || !IsValid(TargetActor) || TargetActor == this
        || TargetActor->IsActorBeingDestroyed() || !SpotLight->bAffectsWorld)
    {
        return false;
    }

    const double Range = SpotLight->AttenuationRadius;
    const double Angle = SpotLight->OuterConeAngle;
    if (!FMath::IsFinite(Range) || Range <= 0.0
        || !FMath::IsFinite(Angle) || Angle <= 0.0 || Angle >= 90.0)
    {
        return false;
    }
    const FVector Origin = SpotLight->GetComponentLocation();
    const FVector Axis = SpotLight->GetForwardVector().GetSafeNormal();
    if (Origin.ContainsNaN() || Axis.ContainsNaN() || Axis.IsNearlyZero()) return false;
    const double Radians = FMath::DegreesToRadians(Angle);
    const double CosAngle = FMath::Cos(Radians);
    const double SinAngle = FMath::Sin(Radians);
    // Contact tolerance: 0.01 cm in world space.
    const double Tolerance = 0.01 / Range;

    TInlineComponentArray<UPrimitiveComponent*> Components;
    TargetActor->GetComponents(Components);
    static const FName IgnoreTag(TEXT("IgnoreSearchlight"));
    static const FName VolumeTag(TEXT("SearchlightVolume"));
    bool bHasExplicitVolumes = false;
    for (const UPrimitiveComponent* Component : Components)
    {
        if (IsValid(Component) && Component->IsRegistered()
            && Component->ComponentHasTag(VolumeTag)
            && !Component->ComponentHasTag(IgnoreTag))
        {
            bHasExplicitVolumes = true;
            break;
        }
    }
    for (const UPrimitiveComponent* Component : Components)
    {
        if (!IsValid(Component) || !Component->IsRegistered()
            || Component->ComponentHasTag(IgnoreTag)) continue;
        const bool bExplicitVolume = Component->ComponentHasTag(VolumeTag);
        if (bHasExplicitVolumes && !bExplicitVolume) continue;
        const FTransform Transform = Component->GetComponentTransform();
        if (Transform.ContainsNaN()) continue;

        SearchlightGeometry::FVolume Box;
        Box.Center = (Component->GetComponentLocation() - Origin) / Range;
        const UShapeComponent* Shape = Cast<UShapeComponent>(Component);
        if (Shape)
        {
            // Explicit test volumes may have NoCollision: the geometry query
            // needs neither physics nor overlaps, and must not block gameplay.
            if (!bExplicitVolume && !Shape->IsCollisionEnabled()) continue;
            if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(Shape))
            {
                const FVector Extent = BoxComponent->GetScaledBoxExtent();
                if (Extent.ContainsNaN() || Extent.GetMin() < 0.0 || Extent.IsNearlyZero()) continue;
                const FQuat Rotation = Transform.GetRotation();
                Box.HalfAxes[0] = Rotation.RotateVector(FVector(Extent.X, 0.0, 0.0)) / Range;
                Box.HalfAxes[1] = Rotation.RotateVector(FVector(0.0, Extent.Y, 0.0)) / Range;
                Box.HalfAxes[2] = Rotation.RotateVector(FVector(0.0, 0.0, Extent.Z)) / Range;
            }
            else if (const USphereComponent* Sphere = Cast<USphereComponent>(Shape))
            {
                const double Radius = Sphere->GetScaledSphereRadius();
                if (!FMath::IsFinite(Radius) || Radius <= 0.0) continue;
                Box.Radius = Radius / Range;
            }
            else if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Shape))
            {
                float Radius = 0.0f;
                float HalfHeight = 0.0f;
                Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
                if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight)
                    || Radius <= 0.0f || HalfHeight < Radius) continue;
                Box.Radius = Radius / Range;
                Box.CapsuleHalfSegment = Capsule->GetUpVector() * ((HalfHeight - Radius) / Range);
            }
            else
            {
                continue;
            }
        }
        else
        {
            // Explicit volumes must be Box/Sphere/Capsule components.
            // Do not silently use mesh bounds in explicit-volume mode.
            if (bHasExplicitVolumes || !Cast<UMeshComponent>(Component)) continue;
            const FBoxSphereBounds LocalBounds = Component->CalcBounds(FTransform::Identity);
            const FVector Extent = LocalBounds.BoxExtent;
            if (LocalBounds.Origin.ContainsNaN() || Extent.ContainsNaN()
                || Extent.GetMin() < 0.0 || Extent.IsNearlyZero()) continue;
            Box.Center = (Transform.TransformPosition(LocalBounds.Origin) - Origin) / Range;
            Box.HalfAxes[0] = Transform.TransformVector(FVector(Extent.X, 0.0, 0.0)) / Range;
            Box.HalfAxes[1] = Transform.TransformVector(FVector(0.0, Extent.Y, 0.0)) / Range;
            Box.HalfAxes[2] = Transform.TransformVector(FVector(0.0, 0.0, Extent.Z)) / Range;
        }
        if (Box.Center.ContainsNaN() || Box.HalfAxes[0].ContainsNaN()
            || Box.HalfAxes[1].ContainsNaN() || Box.HalfAxes[2].ContainsNaN()
            || Box.CapsuleHalfSegment.ContainsNaN() || !FMath::IsFinite(Box.Radius)) continue;

        if (SearchlightGeometry::Intersects(Box, Axis, CosAngle, SinAngle, Tolerance)) return true;
    }
    return false;
}
