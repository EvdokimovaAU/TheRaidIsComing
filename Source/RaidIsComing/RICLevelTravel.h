#pragma once

#include "CoreMinimal.h"

class APlayerController;

namespace RICLevelTravel
{
    void RestartCurrentLevel(APlayerController* Controller);
    void OpenLevel(APlayerController* Controller, FName MapPackage);
    void OpenMainMenu(APlayerController* Controller);
}
