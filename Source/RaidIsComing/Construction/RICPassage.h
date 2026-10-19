#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RICPassage.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UArrowComponent;
class ARICBuilding;

UCLASS()
class RAIDISCOMING_API ARICPassage : public AActor
{
	GENERATED_BODY()

public:
	ARICPassage();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Passage")
	TObjectPtr<UBoxComponent> PassageBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Passage")
	TObjectPtr<UArrowComponent> DirectionArrow;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Passage")
	FGuid PassageId;

	// У проходов одного кольца должно быть одинаковое значение.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passage",
		meta=(ClampMin="0"))
	int32 RingId = 0;

	// Половина размера места под преграду.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Passage",
		meta=(ClampMin="1"))
	FVector HalfSize = FVector(100.0f, 150.0f, 100.0f);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Passage")
	TObjectPtr<ARICBuilding> InstalledBarrier;

	UFUNCTION(BlueprintPure, Category="Passage")
	bool IsOpen() const;

	UFUNCTION(BlueprintCallable, Category="Passage")
	bool RegisterBarrier(ARICBuilding* Barrier);
	// Плоскость, показывающая место установки преграды.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passage|Highlight")
	TObjectPtr<UStaticMeshComponent> HighlightMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Passage|Highlight")
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	UFUNCTION(BlueprintCallable, Category = "Passage|Highlight")
	void SetHighlighted(bool bEnabled);

	// Кнопки для проверки подсветки на выбранном проходе в редакторе.
	UFUNCTION(CallInEditor, Category = "Passage|Highlight")
	void ShowHighlight();

	UFUNCTION(CallInEditor, Category = "Passage|Highlight")
	void HideHighlight();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;

private:
	bool bHighlightRequested = false;

	void RefreshHighlight();
	UFUNCTION()
	void OnBarrierDestroyed(AActor* DestroyedActor);
};