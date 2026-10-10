// Copyright Epic Games, Inc. All Rights Reserved.

#include "RaidIsComingPlayerController.h"

#include "Construction/RICBuilding.h"
#include "Construction/RICConstructionComponent.h"

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

    ConstructionComponent =
        CreateDefaultSubobject<URICConstructionComponent>(
            TEXT("ConstructionComponent"));

    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Default;

    CachedDestination = FVector::ZeroVector;
    FollowTime = 0.0f;
}

void ARaidIsComingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!IsLocalPlayerController() || !InputComponent)
    {
        return;
    }

    // Выбор казармы.
    InputComponent->BindKey(
        EKeys::B,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::StartBarracksPreview
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

    // Отмена строительства.
    InputComponent->BindKey(
        EKeys::RightMouseButton,
        IE_Pressed,
        this,
        &ARaidIsComingPlayerController::CancelBuildingPreview
    );

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
    if (SetDestinationClickAction)
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
            TEXT("SetDestinationClickAction is not assigned.")
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

void ARaidIsComingPlayerController::StartBarracksPreview()
{
    if (ConstructionComponent->StartPreview(BarracksClass))
    {
        StopMovement();

        FollowTime = 0.0f;
        bSuppressDestinationUntilRelease = true;
    }
}

void ARaidIsComingPlayerController::CancelBuildingPreview()
{
    ConstructionComponent->CancelPreview();
}

void ARaidIsComingPlayerController::OnInputStarted()
{
    StopMovement();

    FollowTime = 0.0f;

    bSuppressDestinationUntilRelease =
        ConstructionComponent
        && ConstructionComponent->IsBuildingMode();

    if (bSuppressDestinationUntilRelease)
    {
        ConstructionComponent->TryPlacePreview();
        return;
    }

    UpdateCachedDestination();
}

void ARaidIsComingPlayerController::OnSetDestinationTriggered()
{
    // Во время превью клик не управляет персонажем.
    if (ConstructionComponent->IsBuildingMode()
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
    if (ConstructionComponent->IsBuildingMode()
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

    if (ConstructionComponent->StartPreview(BuildingClass))
    {
        StopMovement();

        FollowTime = 0.0f;
        bSuppressDestinationUntilRelease = true;
    }
}