#include "RICLevelTravel.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void RICLevelTravel::RestartCurrentLevel(APlayerController* Controller)
{
    if (!IsValid(Controller) || !Controller->GetWorld())
    {
        return;
    }

    UWorld* World = Controller->GetWorld();
    const FString MapPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    OpenLevel(Controller, FName(*MapPackage));
}

void RICLevelTravel::OpenLevel(APlayerController* Controller, FName MapPackage)
{
    if (!IsValid(Controller) || !Controller->GetWorld() || MapPackage.IsNone()) return;
    UGameplayStatics::SetGamePaused(Controller, false);
    Controller->SetInputMode(FInputModeGameOnly());
    UGameplayStatics::OpenLevel(Controller, MapPackage, true);
}

void RICLevelTravel::OpenMainMenu(APlayerController* Controller)
{
    OpenLevel(Controller, TEXT("/Game/Levels/Main_Menu"));
}
