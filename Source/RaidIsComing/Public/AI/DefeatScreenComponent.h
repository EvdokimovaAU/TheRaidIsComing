
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DefeatScreenComponent.generated.h"

class UUserWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RAIDISCOMING_API UDefeatScreenComponent
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDefeatScreenComponent();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Defeat")
    TSubclassOf<UUserWidget> DefeatWidgetClass;

    UFUNCTION(BlueprintCallable, Category = "Defeat")
    void ShowDefeat();

private:
    bool bDefeatShown = false;
};
