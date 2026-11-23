// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "RaidIsComingPlayerController.generated.h"
class URICBuildMenuWidget;
class URICMoneyComponent;
class AWaveManager;
class URICStartAttackButton;

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;
class ARICBuilding;
class URICConstructionComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(abstract)
class ARaidIsComingPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ARaidIsComingPlayerController();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Construction")
    TObjectPtr<URICConstructionComponent> ConstructionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Money")
    TObjectPtr<URICMoneyComponent> MoneyComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Construction")
    TSubclassOf<ARICBuilding> ArcherTowerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Construction")
    TSubclassOf<ARICBuilding> MageTowerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Construction")
    TSubclassOf<ARICBuilding> BarrierClass;

    UPROPERTY(VisibleDefaultsOnly, Category = "AI")
    TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

    UPROPERTY(EditAnywhere, Category = "Input")
    float ShortPressThreshold = 0.2f;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UNiagaraSystem> FXCursor;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> SetDestinationClickAction;

    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UInputAction> SetDestinationTouchAction;

    uint32 bMoveToMouseCursor : 1;
    uint32 bIsTouch : 1;

    FVector CachedDestination;
    float FollowTime = 0.0f;

    bool bSuppressDestinationUntilRelease = false;


    virtual void BeginPlay() override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual void SetupInputComponent() override;
    void OnConstructionClick();

    void StartArcherTowerPreview();
    void StartBarrierPreview();

    void StartTowerPreview(TSubclassOf<ARICBuilding> BuildingClass);
    void CancelBuildingPreview();

    void OnInputStarted();
    void OnMouseInputStarted();
    void OnSetDestinationTriggered();
    void OnSetDestinationReleased();
    void OnTouchTriggered();
    void OnTouchReleased();

    void UpdateCachedDestination();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, Category = "Construction|Menu")
    TSubclassOf<URICBuildMenuWidget> BuildMenuWidgetClass;

private:
    bool bIntroTrainingShown = false;
    UPROPERTY(Transient)
    TObjectPtr<class UUserWidget> IntroTraining;
    void HandleTrainingWidgetAdded(class UWidget* Widget, class ULocalPlayer* WidgetPlayer);

    UFUNCTION()
    void FinishIntroTraining(int32 WaveNumber);

    UFUNCTION()
    void ShowSearchlightTraining(bool bNight);

    TSharedPtr<class SWidget> PauseMenu;
    void OpenPauseMenu();
    void RemovePauseMenu();
    void ResumeFromPause();

    friend class URICStartAttackButton;

    TWeakObjectPtr<AWaveManager> StartAttackWaveManager;
    TWeakObjectPtr<URICStartAttackButton> StartAttackButton;
    void RegisterStartAttackButton(URICStartAttackButton* Button);
    void ClearStartAttackButton();
    void SetGameplayInputMode(bool bAllowUI);

    UFUNCTION()
    void HandleStartAttackClicked();

    UFUNCTION()
    void HandleWaveStarted(int32 WaveNumber);

    UPROPERTY(Transient)
    TObjectPtr<URICBuildMenuWidget> BuildMenuWidget;
    TOptional<FVector> PendingBuildPosition;
    bool bConstructionModeActive = false;
    bool bBarrierPreview = false;
    bool bResolvingBuildMenu = false;
    void OpenBuildMenu();
    void CloseBuildMenu();
    void PositionBuildMenu();
    UFUNCTION()
    void HandleTowerSelected(bool bMageTower);
    UFUNCTION()
    void HandleMenuCancelled();
    UFUNCTION()
    void HandlePlacementRejected(const FText& Reason);
};
