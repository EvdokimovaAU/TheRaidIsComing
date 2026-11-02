// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaidIsComingPlayerController.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICBuildZone.h"
#include "EngineUtils.h"
#include "Construction/RICConstructionComponent.h"
#include "Construction/RICMoneyComponent.h"

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
    UE_LOG(LogTemp, Warning,
        TEXT("[RIC Input] BeginPlay Controller=%s Class=%s Local=%d"),
        *GetName(), *GetNameSafe(GetClass()), IsLocalPlayerController());
}

void ARaidIsComingPlayerController::OnConstructionClick()
{
    // Fallback for maps without the old point-and-click Input Action.
    // Outside construction this handler does not move the camera pawn.
    if (ConstructionComponent && ConstructionComponent->IsBuildingMode())
    {
        const bool bPlaced = ConstructionComponent->TryPlacePreview();
        UE_LOG(LogTemp, Warning, TEXT("[RIC Input] LMB placement=%d"), bPlaced);
    }
}

void ARaidIsComingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UE_LOG(LogTemp, Warning,
        TEXT("[RIC Input] Setup Controller=%s Local=%d Input=%s"),
        *GetName(), IsLocalPlayerController(), *GetNameSafe(InputComponent));

    if (!IsLocalPlayerController() || !InputComponent)
    {
        return;
    }

    // Вход в строительство без выбранной постройки.
    InputComponent->BindKey(
        EKeys::E,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::EnterConstructionMode
    );

    InputComponent->BindKey(
        EKeys::One,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::StartArcherTowerPreview);

    InputComponent->BindKey(
        EKeys::Two,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::StartMageTowerPreview);

    InputComponent->BindKey(
        EKeys::Three,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::StartBarrierPreview);

    // Отмена строительства.
    InputComponent->BindKey(
        EKeys::RightMouseButton,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::CancelBuildingPreview
    ).bConsumeInput = false;

    // Diana's pawn also needs RMB for isRotate.
    // Use one mouse placement path: fallback OR the existing Enhanced Input action.
    if (!SetDestinationClickAction || !DefaultMappingContext)
    {
        InputComponent->BindKey(
            EKeys::LeftMouseButton,
            IE_Pressed,
            this,
            &ARaidIsComingPlayerController::OnConstructionClick
        ).bConsumeInput = false;
        UE_LOG(LogTemp, Warning, TEXT("[RIC Input] Direct construction LMB enabled."));
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
            &ARaidIsComingPlayerController::OnInputStarted
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
            TEXT("[RIC Input] Legacy mouse movement disabled: Input Action or Mapping Context missing. Construction uses direct LMB.")
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

void ARaidIsComingPlayerController::EnterConstructionMode()
{
    if (!ConstructionComponent || !GetWorld())
    {
        return;
    }

    ConstructionComponent->CancelPreview();
    bWaitingForBuildingSelection = true;
    StopMovement();
    FollowTime = 0.0f;
    bSuppressDestinationUntilRelease = true;

    // Пока тип не выбран, показываем потенциальные зоны для обеих башен.
    // Проверка размеров и свободного места выполняется после выбора 1/2.
    const ARICBuilding* ArcherDefaults = ArcherTowerClass
        ? ArcherTowerClass->GetDefaultObject<ARICBuilding>() : nullptr;
    const ARICBuilding* MageDefaults = MageTowerClass
        ? MageTowerClass->GetDefaultObject<ARICBuilding>() : nullptr;

    for (TActorIterator<ARICBuildZone> It(GetWorld()); It; ++It)
    {
        const bool bAllowed =
            (ArcherDefaults && It->IsBuildingAllowed(ArcherDefaults->BuildingType))
            || (MageDefaults && It->IsBuildingAllowed(MageDefaults->BuildingType));
        It->SetHighlighted(bAllowed);
    }

    UE_LOG(LogTemp, Log,
        TEXT("[RIC Input] Construction selection: 1 Archer, 2 Mage, 3 Barrier. RMB cancels."));
}

void ARaidIsComingPlayerController::CancelBuildingPreview()
{
    bWaitingForBuildingSelection = false;
    if (ConstructionComponent)
    {
        ConstructionComponent->CancelPreview();
    }
}

void ARaidIsComingPlayerController::OnInputStarted()
{
    StopMovement();

    FollowTime = 0.0f;

    bSuppressDestinationUntilRelease =
        bWaitingForBuildingSelection
        || (ConstructionComponent && ConstructionComponent->IsBuildingMode());

    if (bSuppressDestinationUntilRelease)
    {
        if (!bWaitingForBuildingSelection && ConstructionComponent)
        {
            ConstructionComponent->TryPlacePreview();
        }
        return;
    }

    UpdateCachedDestination();
}

void ARaidIsComingPlayerController::OnSetDestinationTriggered()
{
    // Во время превью клик не управляет персонажем.
    if (bWaitingForBuildingSelection
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
    if (bWaitingForBuildingSelection
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
    StartTowerPreview(ArcherTowerClass);
}

void ARaidIsComingPlayerController::StartMageTowerPreview()
{
    StartTowerPreview(MageTowerClass);
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

    UE_LOG(LogTemp, Warning, TEXT("[RIC Input] Preview requested: %s"),
        *GetNameSafe(BuildingClass.Get()));
    const bool bStarted = ConstructionComponent->StartPreview(BuildingClass);
    if (!bStarted && bWaitingForBuildingSelection)
    {
        EnterConstructionMode();
    }
    UE_LOG(LogTemp, Warning, TEXT("[RIC Input] Preview started=%d (visibility depends on valid zone/passage)."), bStarted);
    if (bStarted)
    {
        bWaitingForBuildingSelection = false;
        StopMovement();

        FollowTime = 0.0f;
        bSuppressDestinationUntilRelease = true;
    }
}
void ARaidIsComingPlayerController::StartBarrierPreview()
{
    UE_LOG(LogTemp, Warning, TEXT("[RIC Input] 3 received."));
    StartTowerPreview(BarrierClass);
}
