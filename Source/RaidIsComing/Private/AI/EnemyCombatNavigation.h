#pragma once

#include "CoreMinimal.h"

class AEnemyAI;
class ARICBuilding;

// Distances are measured from the character centre to actual building collision.
// NavMesh projection alone never establishes reachability.
namespace EnemyCombatNavigation
{
    constexpr float AcceptanceRadius = 8.f;
    bool GetSurfacePoint(const FVector& Position, const ARICBuilding* Building, FVector& Point);
    float SurfaceDistance(const FVector& Position, const ARICBuilding* Building);
    bool CanAttack(const AEnemyAI* Enemy, const ARICBuilding* Building, float Range);
    bool HasCompletePath(AEnemyAI* Enemy, const FVector& Destination);
    bool FindApproach(AEnemyAI* Enemy, const ARICBuilding* Building, float Range,
        FVector& Destination, const TArray<FVector>& Reserved = {},
        float Separation = 0.f, const FVector* GroupAnchor = nullptr,
        float GroupRadius = TNumericLimits<float>::Max());
}
