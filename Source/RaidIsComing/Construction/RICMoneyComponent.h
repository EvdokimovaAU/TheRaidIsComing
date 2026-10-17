#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RICMoneyComponent.generated.h"

UCLASS(ClassGroup=(Construction), meta=(BlueprintSpawnableComponent))
class RAIDISCOMING_API URICMoneyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URICMoneyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Money",
		meta=(ClampMin="0"))
	int32 InitialMoney = 300;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Money")
	int32 CurrentMoney = 0;

	UFUNCTION(BlueprintPure, Category="Money")
	bool CanAfford(int32 Cost) const;

	UFUNCTION(BlueprintCallable, Category="Money")
	bool TrySpend(int32 Cost);

	// Возвращает количество денег, которое фактически добавилось.
	UFUNCTION(BlueprintCallable, Category="Money")
	int32 AddMoney(int32 Amount);

protected:
	virtual void BeginPlay() override;
};