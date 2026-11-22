
#include "AI/VictoryScreenComponent.h"

#include "AI/WaveManager.h"
#include "Construction/RICBuilding.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UVictoryScreenComponent::UVictoryScreenComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVictoryScreenComponent::BeginPlay()
{
    Super::BeginPlay();

    WaveManager = Cast<AWaveManager>(GetOwner());

    if (!IsValid(WaveManager))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] Component must be attached to WaveManager"));
        return;
    }

    WaveManager->OnAllWavesCompleted.AddDynamic(
        this,
        &UVictoryScreenComponent::HandleAllWavesCompleted
    );

    UE_LOG(LogTemp, Log,
        TEXT("[Victory] Subscribed to all waves completed"));
}

void UVictoryScreenComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(WaveManager))
    {
        WaveManager->OnAllWavesCompleted.RemoveDynamic(
            this,
            &UVictoryScreenComponent::HandleAllWavesCompleted
        );
    }

    Super::EndPlay(EndPlayReason);
}

void UVictoryScreenComponent::HandleAllWavesCompleted()
{
    if (bVictoryShown)
    {
        return;
    }

    if (!IsValid(Capitol))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] Capitol reference is not assigned"));
        return;
    }

    if (!Capitol->IsAlive() || Capitol->CurrentHP <= 0.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Victory] Capitol destroyed. Victory cancelled"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Victory] All enemies defeated. Capitol HP: %.1f"),
        Capitol->CurrentHP);

    ShowVictory();
}

void UVictoryScreenComponent::ShowVictory()
{
    if (bVictoryShown || !GetWorld())
    {
        return;
    }

    APlayerController* PC =
        UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(PC))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] PlayerController not found"));
        return;
    }

    if (!VictoryWidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] VictoryWidgetClass not assigned"));
        return;
    }

    UUserWidget* NewWidget =
        CreateWidget<UUserWidget>(
            PC,
            VictoryWidgetClass
        );

    if (!IsValid(NewWidget))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] Failed to create widget"));
        return;
    }

    VictoryWidget = NewWidget;
    bVictoryShown = true;

    VictoryWidget->AddToViewport(100);

    FInputModeUIOnly InputMode;

    InputMode.SetWidgetToFocus(
        VictoryWidget->TakeWidget()
    );

    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock
    );

    PC->SetInputMode(InputMode);
    PC->SetShowMouseCursor(true);

    UGameplayStatics::SetGamePaused(this, true);

    UE_LOG(LogTemp, Warning,
        TEXT("[Victory] VICTORY SCREEN SHOWN"));
}

void UVictoryScreenComponent::OpenLevelByName(
    FName LevelName)
{
    if (LevelName.IsNone())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Victory] Level name not configured"));
        return;
    }

    UGameplayStatics::SetGamePaused(this, false);

    UGameplayStatics::OpenLevel(
        this,
        LevelName
    );
}

void UVictoryScreenComponent::RestartLevel()
{
    if (!GetWorld())
    {
        return;
    }

    OpenLevelByName(
        FName(*GetWorld()->GetName())
    );
}

void UVictoryScreenComponent::LoadNextLevel()
{
    OpenLevelByName(NextLevelName);
}

void UVictoryScreenComponent::LoadMainMenu()
{
    OpenLevelByName(MainMenuLevelName);
}
