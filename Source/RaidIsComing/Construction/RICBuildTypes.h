#pragma once

#include "CoreMinimal.h"
#include "RICBuildTypes.generated.h"

UENUM(BlueprintType)
enum class ERICBuildingType : uint8
{
    Barracks = 0 UMETA(DisplayName = "Barracks (Legacy)"),
    ArcherTower = 1 UMETA(DisplayName = "Archer Tower"),
    Barrier = 2 UMETA(DisplayName = "Barrier"),
    MageTower = 3 UMETA(DisplayName = "Mage Tower")
};