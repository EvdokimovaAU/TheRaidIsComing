#include "EnemyCombatNavigation.h"
#include "AI/EnemyAI.h"
#include "Construction/RICBuilding.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"

namespace EnemyCombatNavigation
{
    static bool IsCombatCollision(const UPrimitiveComponent* Component)
    {
        return IsValid(Component) && Component->IsQueryCollisionEnabled() &&
            Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
    }

    bool GetSurfacePoint(const FVector& Position, const ARICBuilding* Building, FVector& Point)
    {
        if (!IsValid(Building)) return false;
        TInlineComponentArray<UPrimitiveComponent*> Components(Building);
        float BestDistance = TNumericLimits<float>::Max();
        bool bFound = false;
        for (auto* Component : Components)
        {
            if (!IsCombatCollision(Component)) continue;
            FVector Candidate;
            float Distance = Component->GetClosestPointOnCollision(Position, Candidate);
            if (Distance < 0.f || !Component->Bounds.GetBox().ExpandBy(8.f).IsInsideOrOn(Candidate))
            {
                // Complex collision may not support closest-point queries. Some
                // imported meshes also have simple hulls displaced from their
                // visible geometry (including the project's Capitol mesh).
                // Bounds only reject that unusable result; damage still needs
                // a hit on actual collision triangles, never a bounds distance.
                Distance = -1.f;
                FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyComplexSurface), true);
                const FVector Center = Component->Bounds.Origin;
                const FVector Ends[] = { FVector(Center.X, Center.Y, Position.Z), Center };
                for (const FVector& End : Ends)
                {
                    FHitResult Hit;
                    if (Component->LineTraceComponent(Hit, Position, End, Params) && !Hit.bStartPenetrating)
                    {
                        Candidate = Hit.ImpactPoint;
                        Distance = FVector::Distance(Position, Candidate);
                        break;
                    }
                }
            }
            if (Distance > 0.f && Distance < BestDistance)
            {
                BestDistance = Distance;
                Point = Candidate;
                bFound = true;
            }
        }
        return bFound;
    }

    float SurfaceDistance(const FVector& Position, const ARICBuilding* Building)
    {
        FVector Point;
        return GetSurfacePoint(Position, Building, Point)
            ? FVector::Distance(Position, Point) : TNumericLimits<float>::Max();
    }

    static bool CanAttackFrom(const AEnemyAI* Enemy, const FVector& Position,
        const ARICBuilding* Building, float Range)
    {
        FVector Point;
        if (!Enemy || !GetSurfacePoint(Position, Building, Point) ||
            FVector::Distance(Position, Point) > Range) return false;

        FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyAttackSurface), false, Enemy);
        FCollisionObjectQueryParams Objects;
        Objects.AddObjectTypesToQuery(ECC_WorldStatic);
        Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
        FHitResult Hit;
        const FVector End = Point + (Point - Position).GetSafeNormal() * 2.f;
        // Another building in front of the selected surface blocks melee damage.
        return !Enemy->GetWorld()->LineTraceSingleByObjectType(Hit, Position, End, Objects, Params) ||
            Hit.GetActor() == Building;
    }

    bool CanAttack(const AEnemyAI* Enemy, const ARICBuilding* Building, float Range)
    {
        return Enemy && CanAttackFrom(Enemy, Enemy->GetActorLocation(), Building, Range);
    }

    bool HasCompletePath(AEnemyAI* Enemy, const FVector& Destination)
    {
        if (!Enemy || !Enemy->GetWorld()) return false;
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
        if (!Nav) return false;
        auto* Path = Nav->FindPathToLocationSynchronously(Enemy->GetWorld(),
            Enemy->GetActorLocation(), Destination, Enemy);
        return IsValid(Path) && Path->IsValid() && !Path->IsPartial() &&
            !Path->PathPoints.IsEmpty() &&
            FVector::Dist2D(Path->PathPoints[0], Enemy->GetActorLocation()) <= AcceptanceRadius &&
            FVector::Dist2D(Path->PathPoints.Last(), Destination) <= AcceptanceRadius;
    }

    bool FindApproach(AEnemyAI* Enemy, const ARICBuilding* Building, float Range,
        FVector& Destination, const TArray<FVector>& Reserved, float Separation,
        const FVector* GroupAnchor, float GroupRadius)
    {
        if (!Enemy || !IsValid(Building)) return false;
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
        if (!Nav) return false;
        const auto* Capsule = Enemy->GetCapsuleComponent();
        const float Radius = Capsule->GetScaledCapsuleRadius();
        const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        const float Offset = FMath::Max(Radius + 4.f, Range - AcceptanceRadius - 12.f);
        TArray<FVector> Candidates;
        TInlineComponentArray<UPrimitiveComponent*> Components(Building);
        for (const auto* Component : Components)
        {
        if (!IsCombatCollision(Component)) continue;
        const FBox Bounds = Component->CalcBounds(FTransform::Identity).GetBox();
        const FVector Center = Bounds.GetCenter();
        const FVector Extent = Bounds.GetExtent();
        const FTransform Transform = Component->GetComponentTransform();
        for (int32 Side = 0; Side < 4; ++Side)
        for (int32 Index = 0; Index <= 16; ++Index)
        for (int32 Depth = 0; Depth < 2; ++Depth)
        {
            const float T = -1.f + Index / 8.f;
            const float Gap = Offset - Depth * 12.f;
            FVector Candidate = Center;
            FVector Outward = FVector::ZeroVector;
            if (Side < 2)
            {
                Outward.X = Side == 0 ? -1.f : 1.f;
                Candidate.X += Outward.X * Extent.X;
                Candidate.Y += T * Extent.Y;
            }
            else
            {
                Outward.Y = Side == 2 ? -1.f : 1.f;
                Candidate.Y += Outward.Y * Extent.Y;
                Candidate.X += T * Extent.X;
            }
            Candidate = Transform.TransformPosition(Candidate) +
                Transform.TransformVectorNoScale(Outward).GetSafeNormal2D() * Gap;
            Candidate.Z = Enemy->GetActorLocation().Z;
            FNavLocation Projected;
            if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(40.f, 40.f, HalfHeight + 80.f))) continue;
            const FVector Position = Projected.Location + FVector(0.f, 0.f, HalfHeight);
            if (!CanAttackFrom(Enemy, Position, Building, Range - AcceptanceRadius - 2.f)) continue;
            if (GroupAnchor && FVector::Dist2D(Projected.Location, *GroupAnchor) > GroupRadius) continue;
            bool bOccupied = false;
            for (const FVector& Other : Reserved)
                bOccupied |= FVector::Dist2D(Projected.Location, Other) < Separation;
            if (bOccupied) continue;

            FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyAttackPosition), false, Enemy);
            FCollisionObjectQueryParams Objects;
            Objects.AddObjectTypesToQuery(ECC_WorldStatic);
            Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
            if (Enemy->GetWorld()->OverlapAnyTestByObjectType(Position + FVector(0.f, 0.f, 2.f),
                FQuat::Identity, Objects, FCollisionShape::MakeCapsule(Radius, HalfHeight - 2.f), Params)) continue;
            Candidates.Add(Projected.Location);
        }
        }
        Candidates.Sort([Enemy](const FVector& A, const FVector& B)
        {
            return FVector::DistSquared2D(A, Enemy->GetActorLocation()) <
                FVector::DistSquared2D(B, Enemy->GetActorLocation());
        });
        // The nearest surface in straight-line distance may require a long
        // detour around the building. Compare complete path lengths so nearby
        // companions do not choose opposite routes to nearly identical points.
        double BestLength = TNumericLimits<double>::Max();
        bool bFound = false;
        for (const FVector& Candidate : Candidates)
        {
            if (FVector::Dist2D(Enemy->GetActorLocation(), Candidate) >= BestLength) break;
            auto* Path = Nav->FindPathToLocationSynchronously(Enemy->GetWorld(),
                Enemy->GetActorLocation(), Candidate, Enemy);
            if (!IsValid(Path) || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.IsEmpty() ||
                FVector::Dist2D(Path->PathPoints[0], Enemy->GetActorLocation()) > AcceptanceRadius ||
                FVector::Dist2D(Path->PathPoints.Last(), Candidate) > AcceptanceRadius) continue;
            const double Length = Path->GetPathLength();
            if (Length < BestLength)
            {
                BestLength = Length;
                Destination = Candidate;
                bFound = true;
            }
        }
        return bFound;
    }
}
