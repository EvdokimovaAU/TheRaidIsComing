
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VictoryScreenComponent.generated.h"

class UUserWidget;
class ARICBuilding;
class AWaveManager;

UCLASS(ClassGroup = (Custom),
    meta = (BlueprintSpawnableComponent))
    class RAIDISCOMING_API UVictoryScreenComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UVictoryScreenComponent();

    UFUNCTION(BlueprintCallable, Category = "Victory")
    void ShowVictory();

    UFUNCTION(BlueprintCallable, Category = "Victory")
    void RestartLevel();

    UFUNCTION(BlueprintCallable, Category = "Victory")
    void LoadNextLevel();

    UFUNCTION(BlueprintCallable, Category = "Victory")
    void LoadMainMenu();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    // Widget shown after victory
    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Victory|UI")
    TSubclassOf<UUserWidget> VictoryWidgetClass;

    // Capitol placed on the level
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly,
        Category = "Victory|References")
    TObjectPtr<ARICBuilding> Capitol = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Victory|Levels")
    FName NextLevelName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly,
        Category = "Victory|Levels")
    FName MainMenuLevelName = NAME_None;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> VictoryWidget = nullptr;

    UPROPERTY()
    TObjectPtr<AWaveManager> WaveManager = nullptr;

    bool bVictoryShown = false;

    UFUNCTION()
    void HandleAllWavesCompleted();

    void OpenLevelByName(FName LevelName);
};
