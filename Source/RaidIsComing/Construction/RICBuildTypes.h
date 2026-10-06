#pragma once

#include "CoreMinimal.h"
#include "RICBuildTypes.generated.h"

UENUM(BlueprintType)
enum class ERICBuildingType : uint8
{
    Barracks    UMETA(DisplayName = "Barracks"),
    ArcherTower UMETA(DisplayName = "Archer Tower"),
    Barrier     UMETA(DisplayName = "Barrier")
};