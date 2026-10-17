// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "RaidIsComingPlayerController.generated.h"
class URICMoneyComponent;

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
    TSubclassOf<ARICBuilding> BarracksClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Construction")
    TSubclassOf<ARICBuilding> ArcherTowerClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Construction")
    TSubclassOf<ARICBuilding> MageTowerClass;

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

    virtual void SetupInputComponent() override;

    void StartBarracksPreview();
    void StartArcherTowerPreview();
    void StartMageTowerPreview();

    void StartTowerPreview(TSubclassOf<ARICBuilding> BuildingClass);
    void CancelBuildingPreview();

    void OnInputStarted();
    void OnSetDestinationTriggered();
    void OnSetDestinationReleased();
    void OnTouchTriggered();
    void OnTouchReleased();

    void UpdateCachedDestination();
};