// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaidIsComingPlayerController.h"
#include "AI/WaveManager.h"

#include "Construction/RICBuilding.h"
#include "UI/RICBuildMenuWidget.h"
#include "UI/RICStartAttackButton.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Construction/RICConstructionComponent.h"
#include "Construction/RICMoneyComponent.h"
#include "Construction/RICPassage.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RaidIsComing.h"

ARaidIsComingPlayerController::ARaidIsComingPlayerController()
{
    bIsTouch = false;
    bMoveToMouseCursor = false;

    PathFollowingComponent =
        CreateDefaultSubobject<UPathFollowingComponent>(
            TEXT("Path Following Component"));

    MoneyComponent = CreateDefaultSubobject<URICMoneyComponent>(
        TEXT("MoneyComponent")
    );

    ConstructionComponent =
        CreateDefaultSubobject<URICConstructionComponent>(
            TEXT("ConstructionComponent"));

    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;

    CachedDestination = FVector::ZeroVector;
    FollowTime = 0.0f;
}

void ARaidIsComingPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalPlayerController())
    {
        for (TObjectIterator<UWidgetComponent> Widget; Widget; ++Widget)
        {
            const UClass* WidgetClass = Widget->GetWidgetClass();
            if (Widget->GetWorld() == GetWorld() && WidgetClass &&
                WidgetClass->GetPathName() == TEXT("/Game/UI/WBP_Arrow.WBP_Arrow_C"))
            {
                // Level instances may override the tutorial Blueprint defaults.
                Widget->SetBackgroundColor(FLinearColor::Transparent);
                Widget->SetBlendMode(EWidgetBlendMode::Transparent);
                Widget->SetMaterial(0, nullptr);
            }
        }
    }
    ConstructionComponent->OnPlacementRejected.AddUniqueDynamic(this, &ThisClass::HandlePlacementRejected);
}

void ARaidIsComingPlayerController::OnConstructionClick()
{
    if (!ConstructionComponent || !bConstructionModeActive || bResolvingBuildMenu)
    {
        return;
    }
    if (BuildMenuWidget)
    {
        HandleMenuCancelled();
        return;
    }
    if (bBarrierPreview)
    {
        if (ConstructionComponent->TryPlacePreview())
        {
            bConstructionModeActive = false;
            bBarrierPreview = false;
            ConstructionComponent->SetPreviewHighlightTarget(nullptr);
        }

        return;
    }
    OpenBuildMenu();
}

void ARaidIsComingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!IsLocalPlayerController() || !InputComponent)
    {
        return;
    }

    // Отмена строительства.
    InputComponent->BindKey(
        EKeys::RightMouseButton,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::CancelBuildingPreview
    ).bConsumeInput = false;

    // RMB также используется для поворота камеры.
    if (!SetDestinationClickAction || !DefaultMappingContext)
    {
        InputComponent->BindKey(
            EKeys::LeftMouseButton,
            IE_Pressed,
            this,
            &ARaidIsComingPlayerController::OnMouseInputStarted
        ).bConsumeInput = false;
    }

    // Контекст ввода для обычного движения.
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
        else
        {
            UE_LOG(
                LogRaidIsComing,
                Warning,
                TEXT("DefaultMappingContext is not assigned.")
            );
        }
    }

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(InputComponent);

    if (!EnhancedInputComponent)
    {
        UE_LOG(
            LogRaidIsComing,
            Error,
            TEXT("'%s': Enhanced Input Component was not found."),
            *GetNameSafe(this)
        );

        return;
    }

    // Управление мышью.
    if (SetDestinationClickAction && DefaultMappingContext)
    {
        EnhancedInputComponent->BindAction(
            SetDestinationClickAction,
            ETriggerEvent::Started,
            this,
            &ARaidIsComingPlayerController::OnMouseInputStarted
        );

        EnhancedInputComponent->BindAction(
            SetDestinationClickAction,
            ETriggerEvent::Triggered,
            this,
            &ARaidIsComingPlayerController::OnSetDestinationTriggered
        );

        EnhancedInputComponent->BindAction(
            SetDestinationClickAction,
            ETriggerEvent::Completed,
            this,
            &ARaidIsComingPlayerController::OnSetDestinationReleased
        );

        EnhancedInputComponent->BindAction(
            SetDestinationClickAction,
            ETriggerEvent::Canceled,
            this,
            &ARaidIsComingPlayerController::OnSetDestinationReleased
        );
    }
    else
    {
        UE_LOG(
            LogRaidIsComing,
            Warning,
            TEXT("Legacy mouse movement disabled: Input Action or Mapping Context missing. Construction uses direct LMB.")
        );
    }

    // Сенсорное управление.
    if (SetDestinationTouchAction)
    {
        EnhancedInputComponent->BindAction(
            SetDestinationTouchAction,
            ETriggerEvent::Started,
            this,
            &ARaidIsComingPlayerController::OnInputStarted
        );

        EnhancedInputComponent->BindAction(
            SetDestinationTouchAction,
            ETriggerEvent::Triggered,
            this,
            &ARaidIsComingPlayerController::OnTouchTriggered
        );

        EnhancedInputComponent->BindAction(
            SetDestinationTouchAction,
            ETriggerEvent::Completed,
            this,
            &ARaidIsComingPlayerController::OnTouchReleased
        );

        EnhancedInputComponent->BindAction(
            SetDestinationTouchAction,
            ETriggerEvent::Canceled,
            this,
            &ARaidIsComingPlayerController::OnTouchReleased
        );
    }
}

void ARaidIsComingPlayerController::OnMouseInputStarted()
{
    if (bResolvingBuildMenu)
    {
        return;
    }
    if (ConstructionComponent)
    {
        AActor* Target = ConstructionComponent->GetConstructionTargetUnderCursor(
            ArcherTowerClass, BarrierClass);
        if (Target && !Target->IsA<ARICPassage>())
        {
            CancelBuildingPreview();
            ConstructionComponent->SetPreviewHighlightTarget(Target);
            StartArcherTowerPreview();
            if (bConstructionModeActive)
            {
                OpenBuildMenu();
            }
            if (!BuildMenuWidget)
            {
                CancelBuildingPreview();
            }
            return;
        }

        if (!Target && (BuildMenuWidget || bConstructionModeActive || ConstructionComponent->IsBuildingMode()))
        {
            CancelBuildingPreview();
            bSuppressDestinationUntilRelease = true;
            return;
        }

        if (Target && (!bBarrierPreview || !ConstructionComponent->IsBuildingMode()))
        {
            CancelBuildingPreview();
            ConstructionComponent->SetPreviewHighlightTarget(Target);
            StartBarrierPreview();
            if (bConstructionModeActive)
            {
                return;
            }
            bBarrierPreview = false;
            ConstructionComponent->SetPreviewHighlightTarget(nullptr);
        }
        else if (Target && bBarrierPreview)
        {
            ConstructionComponent->SetPreviewHighlightTarget(Target);
        }
    }
    if (bBarrierPreview && bConstructionModeActive)
    {
        OnConstructionClick();
        return;
    }

    if (SetDestinationClickAction && DefaultMappingContext)
    {
        OnInputStarted();
    }
    else
    {
        OnConstructionClick();
    }
}

void ARaidIsComingPlayerController::CancelBuildingPreview()
{
    if (bResolvingBuildMenu)
    {
        return;
    }
    if (BuildMenuWidget)
    {
        CloseBuildMenu();
    }
    bConstructionModeActive = false;
    bBarrierPreview = false;
    if (ConstructionComponent)
    {
        ConstructionComponent->CancelPreview();
        ConstructionComponent->SetPreviewHighlightTarget(nullptr);
    }
}

void ARaidIsComingPlayerController::OnInputStarted()
{
    StopMovement();

    FollowTime = 0.0f;

    bSuppressDestinationUntilRelease =
        bConstructionModeActive
        || (ConstructionComponent && ConstructionComponent->IsBuildingMode());

    if (bSuppressDestinationUntilRelease)
    {
        OnConstructionClick();
        return;
    }

    UpdateCachedDestination();
}

void ARaidIsComingPlayerController::OnSetDestinationTriggered()
{
    // Во время превью клик не управляет персонажем.
    if (bConstructionModeActive
        || (ConstructionComponent && ConstructionComponent->IsBuildingMode())
        || bSuppressDestinationUntilRelease)
    {
        return;
    }

    FollowTime += GetWorld()->GetDeltaSeconds();

    UpdateCachedDestination();

    if (APawn* ControlledPawn = GetPawn())
    {
        const FVector WorldDirection =
            (CachedDestination - ControlledPawn->GetActorLocation())
            .GetSafeNormal();

        ControlledPawn->AddMovementInput(
            WorldDirection,
            1.0f,
            false
        );
    }
}

void ARaidIsComingPlayerController::OnSetDestinationReleased()
{
    if (bConstructionModeActive
        || (ConstructionComponent && ConstructionComponent->IsBuildingMode())
        || bSuppressDestinationUntilRelease)
    {
        FollowTime = 0.0f;
        bSuppressDestinationUntilRelease = false;
        return;
    }

    if (FollowTime <= ShortPressThreshold)
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(
            this,
            CachedDestination
        );

        if (FXCursor)
        {
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                this,
                FXCursor,
                CachedDestination,
                FRotator::ZeroRotator,
                FVector(1.0f),
                true,
                true,
                ENCPoolMethod::None,
                true
            );
        }
    }

    FollowTime = 0.0f;
}

void ARaidIsComingPlayerController::OnTouchTriggered()
{
    bIsTouch = true;
    OnSetDestinationTriggered();
}

void ARaidIsComingPlayerController::OnTouchReleased()
{
    bIsTouch = false;
    OnSetDestinationReleased();
}

void ARaidIsComingPlayerController::UpdateCachedDestination()
{
    FHitResult Hit;
    bool bHitSuccessful = false;

    if (bIsTouch)
    {
        bHitSuccessful = GetHitResultUnderFinger(
            ETouchIndex::Touch1,
            ECC_Visibility,
            true,
            Hit
        );
    }
    else
    {
        bHitSuccessful = GetHitResultUnderCursor(
            ECC_Visibility,
            true,
            Hit
        );
    }

    if (bHitSuccessful)
    {
        CachedDestination = Hit.Location;
    }
}
void ARaidIsComingPlayerController::StartArcherTowerPreview()
{
    bBarrierPreview = false;
    StartTowerPreview(ArcherTowerClass);
}

void ARaidIsComingPlayerController::StartTowerPreview(
    TSubclassOf<ARICBuilding> BuildingClass)
{
    if (!ConstructionComponent)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cannot select tower: ConstructionComponent is missing."));

        return;
    }

    if (!BuildingClass)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Cannot select tower: assign the tower class in BP_RICPlayerController."));

        return;
    }

    const bool bStarted = ConstructionComponent->StartPreview(BuildingClass);
    if (bStarted)
    {
        bConstructionModeActive = true;
        StopMovement();

        FollowTime = 0.0f;
        bSuppressDestinationUntilRelease = true;
    }
}
void ARaidIsComingPlayerController::StartBarrierPreview()
{
    CloseBuildMenu();
    bBarrierPreview = true;
    StartTowerPreview(BarrierClass);
}

void ARaidIsComingPlayerController::RegisterStartAttackButton(URICStartAttackButton* Button)
{
    if (!GetLocalPlayer() || !GetWorld())
    {
        Button->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    AWaveManager* Manager = nullptr;
    for (TActorIterator<AWaveManager> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It))
        {
            continue;
        }
        if (Manager)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Multiple WaveManagers found. Keep one manager on the level."));
            Button->SetVisibility(ESlateVisibility::Collapsed);
            return;
        }
        Manager = *It;
    }

    if (!Manager || Manager->LevelPhase != ERICLevelPhase::Preparation ||
        Manager->GetTotalWaves() == 0)
    {
        Button->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    ClearStartAttackButton();
    StartAttackWaveManager = Manager;
    StartAttackButton = Button;
    Manager->OnWaveStarted.AddUniqueDynamic(this, &ThisClass::HandleWaveStarted);
    Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStartAttackClicked);
    Button->SetVisibility(ESlateVisibility::Visible);
    SetGameplayInputMode(true);
}

void ARaidIsComingPlayerController::HandleStartAttackClicked()
{
    if (AWaveManager* Manager = StartAttackWaveManager.Get())
    {
        Manager->StartNextWave();
    }
}

void ARaidIsComingPlayerController::HandleWaveStarted(int32)
{
    ClearStartAttackButton();
    if (!BuildMenuWidget)
    {
        SetGameplayInputMode(false);
    }
}

void ARaidIsComingPlayerController::ClearStartAttackButton()
{
    if (AWaveManager* Manager = StartAttackWaveManager.Get())
    {
        Manager->OnWaveStarted.RemoveDynamic(this, &ThisClass::HandleWaveStarted);
    }
    StartAttackWaveManager.Reset();

    if (URICStartAttackButton* Button = StartAttackButton.Get())
    {
        Button->OnClicked.RemoveDynamic(this, &ThisClass::HandleStartAttackClicked);
        Button->SetVisibility(ESlateVisibility::Collapsed);
    }
    StartAttackButton.Reset();
}

void ARaidIsComingPlayerController::SetGameplayInputMode(bool bAllowUI)
{
    if (bAllowUI)
    {
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
    else
    {
        FInputModeGameOnly Mode;
        Mode.SetConsumeCaptureMouseDown(false);
        SetInputMode(Mode);
    }
    bShowMouseCursor = true;
}

void ARaidIsComingPlayerController::OpenBuildMenu()
{
    if (!BuildMenuWidgetClass)
    {
        HandlePlacementRejected(NSLOCTEXT("Construction", "MissingMenu", "Assign WBP_BuildMenu in PlayerController Class Defaults."));
        return;
    }
    FVector GroundPosition;
    if (!ConstructionComponent->FreezePreview(GroundPosition)) return;
    PendingBuildPosition = GroundPosition;
    BuildMenuWidget = CreateWidget<URICBuildMenuWidget>(this, BuildMenuWidgetClass);
    if (!BuildMenuWidget)
    {
        PendingBuildPosition.Reset();
        StartArcherTowerPreview();
        return;
    }
    BuildMenuWidget->OnTowerSelected.AddUniqueDynamic(this, &ThisClass::HandleTowerSelected);
    BuildMenuWidget->OnCancelled.AddUniqueDynamic(this, &ThisClass::HandleMenuCancelled);
    BuildMenuWidget->RefreshCosts(ArcherTowerClass, MageTowerClass);
    BuildMenuWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    BuildMenuWidget->AddToViewport(10);
    PositionBuildMenu();
    if (!BuildMenuWidget) return;
    SetGameplayInputMode(true);
}

void ARaidIsComingPlayerController::PositionBuildMenu()
{
    if (!BuildMenuWidget || !PendingBuildPosition.IsSet()) return;
    FVector2D Point;
    if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        this, PendingBuildPosition.GetValue(), Point, true))
    {
        HandleMenuCancelled();
        return;
    }
    BuildMenuWidget->ForceLayoutPrepass();
    const FVector2D Size = BuildMenuWidget->GetDesiredSize();
    const FBox2D Bounds = BuildMenuWidget->GetMenuBounds(Size);
    const FVector2D VisibleSize = Bounds.GetSize();
    int32 Width, Height;
    GetViewportSize(Width, Height);
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
    const FVector2D ViewSize(Width / Scale, Height / Scale);
    FVector2D TopLeft = Point - FVector2D(VisibleSize.X * 0.5, VisibleSize.Y + 16.0);
    TopLeft.X = FMath::Clamp(TopLeft.X, 0.0, FMath::Max(0.0, ViewSize.X - VisibleSize.X));
    TopLeft.Y = FMath::Clamp(TopLeft.Y, 0.0, FMath::Max(0.0, ViewSize.Y - VisibleSize.Y));
    BuildMenuWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
    BuildMenuWidget->SetDesiredSizeInViewport(Size);
    BuildMenuWidget->SetPositionInViewport(TopLeft - Bounds.Min, false);
}

void ARaidIsComingPlayerController::CloseBuildMenu()
{
    if (BuildMenuWidget)
    {
        BuildMenuWidget->OnTowerSelected.RemoveAll(this);
        BuildMenuWidget->OnCancelled.RemoveAll(this);
        BuildMenuWidget->RemoveFromParent();
        BuildMenuWidget = nullptr;
        SetGameplayInputMode(StartAttackButton.IsValid());
    }
    PendingBuildPosition.Reset();
    bSuppressDestinationUntilRelease = true;
}

void ARaidIsComingPlayerController::HandleTowerSelected(bool bMageTower)
{
    if (!BuildMenuWidget || !PendingBuildPosition.IsSet() || bResolvingBuildMenu
        || !bConstructionModeActive || !ConstructionComponent) return;
    TGuardValue<bool> ResolvingGuard(bResolvingBuildMenu, true);
    const FVector Position = PendingBuildPosition.GetValue();
    CloseBuildMenu();
    ConstructionComponent->TryPlaceAt(bMageTower ? MageTowerClass : ArcherTowerClass, Position);
    ConstructionComponent->CancelPreview();
    ConstructionComponent->SetPreviewHighlightTarget(nullptr);
    bConstructionModeActive = false;
}

void ARaidIsComingPlayerController::HandleMenuCancelled()
{
    if (BuildMenuWidget)
    {
        CancelBuildingPreview();
    }
}

void ARaidIsComingPlayerController::HandlePlacementRejected(const FText& Reason)
{
    if (GEngine) GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.0f, FColor::Red, Reason.ToString());
}

void ARaidIsComingPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearStartAttackButton();
    CloseBuildMenu();
    Super::EndPlay(EndPlayReason);
}
