
#include "AI/DefeatScreenComponent.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "RICLevelTravel.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UDefeatScreenComponent::UDefeatScreenComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDefeatScreenComponent::ShowDefeat()
{
    if (bDefeatShown)
    {
        return;
    }

    APlayerController* PC =
        Cast<APlayerController>(GetOwner());

    if (!PC || !DefeatWidgetClass)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Defeat] Controller or widget class missing"));
        return;
    }

    UUserWidget* Widget =
        CreateWidget<UUserWidget>(PC, DefeatWidgetClass);

    if (!Widget)
    {
        return;
    }

    bDefeatShown = true;

    Widget->AddToViewport(100);

    if (UButton* RetryButton = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Reset"))))
    {
        // WBP_Defeat already binds this button to its own level travel.
        RetryButton->OnClicked.RemoveAll(Widget);
        RetryButton->OnClicked.AddDynamic(this, &ThisClass::RetryLevel);
    }

    PC->bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(Widget->TakeWidget());
    PC->SetInputMode(InputMode);

    UGameplayStatics::SetGamePaused(GetWorld(), true);

    UE_LOG(LogTemp, Warning,
        TEXT("[Game] DEFEAT: Capitol destroyed"));
}

void UDefeatScreenComponent::RetryLevel()
{
    RICLevelTravel::RestartCurrentLevel(Cast<APlayerController>(GetOwner()));
}
