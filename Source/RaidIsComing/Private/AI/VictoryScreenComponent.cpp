
#include "AI/VictoryScreenComponent.h"

#include "AI/WaveManager.h"
#include "RICLevelTravel.h"
#include "Construction/RICBuilding.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

UVictoryScreenComponent::UVictoryScreenComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    static ConstructorHelpers::FObjectFinder<UTexture2D> Texture(TEXT("/Game/Assets/UI/NextLevel.NextLevel"));
    NextLevelTexture = Texture.Object;
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
    VictoryWidget->AddToViewport(100);

    if (auto* Button = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("NextLevel"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddDynamic(this, &UVictoryScreenComponent::LoadNextLevel);
        Button->SetIsEnabled(!NextLevelName.IsNone());
        if (NextLevelName.IsNone())
        {
            Button->SetVisibility(ESlateVisibility::Collapsed);
        }
        else if (NextLevelTexture)
        {
            VictoryWidget->ForceLayoutPrepass();
            auto* Menu = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("ToMenu")));
            const FVector2D Size = Menu ? Menu->GetDesiredSize() : FVector2D(476, 109);
            Button->SetVisibility(ESlateVisibility::Visible);
            auto* Image = VictoryWidget->WidgetTree->ConstructWidget<UImage>();
            FSlateBrush Artwork;
            Artwork.SetResourceObject(NextLevelTexture);
            Artwork.SetImageSize(Size);
            Image->SetBrush(Artwork);
            Image->SetVisibility(ESlateVisibility::HitTestInvisible);
            FButtonStyle Style = Button->GetStyle();
            for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed })
            {
                Brush->SetResourceObject(nullptr);
                Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
                Brush->TintColor = FLinearColor::White;
                Brush->SetImageSize(Size);
            }
            Style.NormalPadding = Style.PressedPadding = FMargin(0);
            if (auto* Slot = Cast<UButtonSlot>(Button->SetContent(Image)))
            {
                Slot->SetPadding(FMargin(0));
                Slot->SetHorizontalAlignment(HAlign_Fill);
                Slot->SetVerticalAlignment(VAlign_Fill);
            }
            Button->SetStyle(Style);

        }
    }
    if (auto* Button = Cast<UButton>(VictoryWidget->GetWidgetFromName(TEXT("ToMenu"))))
    {
        Button->OnClicked.Clear();
        Button->OnClicked.AddDynamic(this, &UVictoryScreenComponent::LoadMainMenu);
    }

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
