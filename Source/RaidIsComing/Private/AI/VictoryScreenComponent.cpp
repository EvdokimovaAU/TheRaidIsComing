
#include "AI/VictoryScreenComponent.h"

#include "AI/WaveManager.h"
#include "RICLevelTravel.h"
#include "Construction/RICBuilding.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UVictoryScreenComponent::UVictoryScreenComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVictoryScreenComponent::BeginPlay()
{
    Super::BeginPlay();

    WaveManager = Cast<AWaveManager>(GetOwner());
    const FString MapName = UGameplayStatics::GetCurrentLevelName(this);
    if (MapName == TEXT("Level_1")) NextLevelName = TEXT("/Game/Levels/Level_2");
    else if (MapName == TEXT("Level_2")) NextLevelName = TEXT("/Game/Levels/Level_3");
    else if (MapName == TEXT("Level_3")) NextLevelName = NAME_None;

    if (!IsValid(WaveManager))
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Victory] Component must be attached to WaveManager"));
        return;
    }

    if (!IsValid(Capitol))
    {
        for (TActorIterator<ARICBuilding> Building(GetWorld()); Building; ++Building)
        {
            if (Building->BuildingType == ERICBuildingType::Capitol)
            {
                Capitol = *Building;
                break;
            }
        }
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
        VictoryWidgetClass = LoadClass<UUserWidget>(nullptr,
            TEXT("/Game/UI/All_WBP/WBP_VictoryScreen.WBP_VictoryScreen_C"));
        if (!VictoryWidgetClass) return;
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

    if (auto* Button = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("NextLevel"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddDynamic(this, &UVictoryScreenComponent::LoadNextLevel);
        Button->SetIsEnabled(!NextLevelName.IsNone());
        if (NextLevelName.IsNone())
        {
            Button->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            auto* Label = VictoryWidget->WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetText(NSLOCTEXT("Victory", "Continue", "Продолжить"));
            Button->SetContent(Label);
            UE_LOG(LogTemp, Log, TEXT("[Victory] Continue button label: %s"),
                *Label->GetText().ToString());
        }
    }
    if (auto* Button = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("ToMenu"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddDynamic(this, &UVictoryScreenComponent::LoadMainMenu);
    }

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

    RICLevelTravel::OpenLevel(UGameplayStatics::GetPlayerController(this, 0), LevelName);
}

void UVictoryScreenComponent::RestartLevel()
{
    RICLevelTravel::RestartCurrentLevel(UGameplayStatics::GetPlayerController(this, 0));
}

void UVictoryScreenComponent::LoadNextLevel()
{
    if (bVictoryShown && !NextLevelName.IsNone()) OpenLevelByName(NextLevelName);
}

void UVictoryScreenComponent::LoadMainMenu()
{
    if (MainMenuLevelName.IsNone())
        RICLevelTravel::OpenMainMenu(UGameplayStatics::GetPlayerController(this, 0));
    else
        OpenLevelByName(MainMenuLevelName);
}
