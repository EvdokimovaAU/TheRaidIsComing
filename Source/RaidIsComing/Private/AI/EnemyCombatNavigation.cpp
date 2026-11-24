#include "EnemyCombatNavigation.h"
#include "AI/EnemyAI.h"
#include "Construction/RICBuilding.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace EnemyCombatNavigation
{
    void FaceTarget(AEnemyAI* Enemy, const AActor* Target)
    {
        if (!IsValid(Enemy) || !IsValid(Target)) return;
        const FVector Direction = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
        if (Direction.IsNearlyZero()) return;
        const FRotator Facing = Direction.Rotation();
        Enemy->SetActorRotation(Facing);
        if (auto* Controller = Enemy->GetController()) Controller->SetControlRotation(Facing);
    }
    static bool IsCombatCollision(const UPrimitiveComponent* Component)
    {
        return IsValid(Component) && Component->IsQueryCollisionEnabled() &&
            Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
    }

    // Some Capitol blueprints use a visual mesh with collision disabled.
    // In that case its local footprint supplies the melee surface and attack slots.
    static const UStaticMeshComponent* GetVisualCapitolSurface(const ARICBuilding* Building,
        const TInlineComponentArray<UPrimitiveComponent*>& Components)
    {
        if (Building->BuildingType != ERICBuildingType::Capitol ||
            !IsValid(Building->BuildingMesh) || !Building->BuildingMesh->GetStaticMesh()) return nullptr;
        for (const auto* Component : Components)
            if (IsCombatCollision(Component)) return nullptr;
        return Building->BuildingMesh;
    }

    bool GetSurfacePoint(const FVector& Position, const ARICBuilding* Building, FVector& Point)
    {
        if (!IsValid(Building)) return false;
        TInlineComponentArray<UPrimitiveComponent*> Components(Building);
        if (const auto* Mesh = GetVisualCapitolSurface(Building, Components))
        {
            const FTransform& Transform = Mesh->GetComponentTransform();
            const FVector Local = Transform.InverseTransformPosition(Position);
            const FBox Box = Mesh->CalcBounds(FTransform::Identity).GetBox();
            Point = Transform.TransformPosition(Box.GetClosestPointTo(Local));
            return true;
        }
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
                const FVector Extent = Component->Bounds.BoxExtent;
                const FVector Ends[] = {
                    FVector(Center.X, Center.Y, Position.Z), Center,
                    Center + FVector(0.f, 0.f, Extent.Z),
                    Center - FVector(0.f, 0.f, Extent.Z),
                    Center + FVector(0.f, Extent.Y, 0.f),
                    Center - FVector(0.f, Extent.Y, 0.f),
                    Center + FVector(Extent.X, 0.f, 0.f),
                    Center - FVector(Extent.X, 0.f, 0.f)
                };
                float BestRayDistance = TNumericLimits<float>::Max();
                for (const FVector& End : Ends)
                {
                    FHitResult Hit;
                    if (Component->LineTraceComponent(Hit, Position, End, Params) && !Hit.bStartPenetrating)
                    {
                        const float RayDistance = FVector::Distance(Position, Hit.ImpactPoint);
                        if (RayDistance < BestRayDistance)
                        {
                            BestRayDistance = RayDistance;
                            Candidate = Hit.ImpactPoint;
                            Distance = RayDistance;
                        }
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
        const FVector Start = Enemy->GetActorLocation();
        const ANavigationData* NavData = Nav->GetNavDataForProps(Enemy->GetNavAgentPropertiesRef(), Start);
        if (!NavData) return false;
        TArray<FNavLocation> Candidates;
        TMap<NavNodeRef, bool> ReachablePolygons;
        TSet<FVector> Seen;
        TInlineComponentArray<UPrimitiveComponent*> Components(Building);
        const auto* VisualSurface = GetVisualCapitolSurface(Building, Components);
        for (const auto* Component : Components)
        {
        if (Component != VisualSurface && !IsCombatCollision(Component)) continue;
        const FBox Bounds = Component->CalcBounds(FTransform::Identity).GetBox();
        const FVector Center = Bounds.GetCenter();
        const FVector Extent = Bounds.GetExtent();
        const FTransform Transform = Component->GetComponentTransform();
        for (int32 Side = 0; Side < 4; ++Side)
        for (int32 Index = 0; Index <= 16; ++Index)
        for (int32 Depth = 0; Depth < 3; ++Depth)
        {
            const float T = -1.f + Index / 8.f;
            const float Gap = FMath::Lerp(Offset, Radius + 4.f, Depth * 0.5f);
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
            if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(40.f, 40.f, HalfHeight + 80.f), NavData)) continue;
            if (GroupAnchor && FVector::Dist2D(Projected.Location, *GroupAnchor) > GroupRadius) continue;
            bool bOccupied = false;
            for (const FVector& Other : Reserved)
                bOccupied |= FVector::Dist2D(Projected.Location, Other) < Separation;
            if (bOccupied) continue;

            bool bAlreadySeen;
            Seen.Add(Projected.Location, &bAlreadySeen);
            if (!bAlreadySeen) Candidates.Add(Projected);
        }
        }
        Candidates.Sort([Enemy](const FNavLocation& A, const FNavLocation& B)
        {
            return FVector::DistSquared2D(A.Location, Enemy->GetActorLocation()) <
                FVector::DistSquared2D(B.Location, Enemy->GetActorLocation());
        });
        // The nearest surface in straight-line distance may require a long
        // detour around the building. Compare complete path lengths so nearby
        // companions do not choose opposite routes to nearly identical points.
        double BestLength = TNumericLimits<double>::Max();
        bool bFound = false;
        for (const FNavLocation& Projected : Candidates)
        {
            const FVector& Candidate = Projected.Location;
            if (FVector::Dist2D(Start, Candidate) >= BestLength) break;
            // All slots on a disconnected polygon can be discarded together.
            if (Projected.NodeRef != INVALID_NAVNODEREF)
            {
                if (!ReachablePolygons.Contains(Projected.NodeRef))
                {
                    FPathFindingQuery Query(Enemy, *NavData, Start, Candidate);
                    Query.SetAllowPartialPaths(false);
                    ReachablePolygons.Add(Projected.NodeRef, Nav->TestPathSync(Query));
                }
                if (!ReachablePolygons[Projected.NodeRef]) continue;
            }
            FVector Position = Candidate + FVector(0.f, 0.f, HalfHeight);
            // Recast's voxel height is not the floor height used by CharacterMovement.
            // Validate melee reach at the standing capsule's actual height, not
            // several centimetres above it (important around doors and sloped roofs).
            FFindFloorResult Floor;
            Enemy->GetCharacterMovement()->FindFloor(Position, Floor, false);
            if (!Floor.IsWalkableFloor()) continue;
            Position.Z += (UCharacterMovementComponent::MIN_FLOOR_DIST +
                UCharacterMovementComponent::MAX_FLOOR_DIST) * 0.5f - Floor.GetDistanceToFloor();
            if (!CanAttackFrom(Enemy, Position, Building, Range - AcceptanceRadius - 2.f)) continue;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyAttackPosition), false, Enemy);
            FCollisionObjectQueryParams Objects;
            Objects.AddObjectTypesToQuery(ECC_WorldStatic);
            Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
            if (Enemy->GetWorld()->OverlapAnyTestByObjectType(Position + FVector(0.f, 0.f, 2.f),
                FQuat::Identity, Objects, FCollisionShape::MakeCapsule(Radius, HalfHeight - 2.f), Params)) continue;
            auto* Path = Nav->FindPathToLocationSynchronously(Enemy->GetWorld(),
                Enemy->GetActorLocation(), Candidate, Enemy);
            if (!IsValid(Path) || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.IsEmpty() ||
                FVector::Dist2D(Path->PathPoints[0], Enemy->GetActorLocation()) > AcceptanceRadius ||
                FVector::Dist2D(Path->PathPoints.Last(), Candidate) > AcceptanceRadius) continue;
            // Path following may stop anywhere in its acceptance circle. Door
            // openings make a ray-based surface query discontinuous, so validate
            // that circle rather than treating the exact goal as sufficient.
            bool bStableReach = true;
            for (int32 Probe = 0; Probe < 8; ++Probe)
            {
                const float Angle = Probe * PI / 4.f;
                const FVector OffsetProbe(FMath::Cos(Angle) * AcceptanceRadius,
                    FMath::Sin(Angle) * AcceptanceRadius, 0.f);
                if (!CanAttackFrom(Enemy, Position + OffsetProbe, Building, Range))
                { bStableReach = false; break; }
            }
            if (!bStableReach) continue;
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
