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
    UGameplayStatics::SetGamePaused(World, false);
    Controller->SetInputMode(FInputModeGameOnly());
    UGameplayStatics::OpenLevel(World, FName(*MapPackage), true);
}
