
#pragma once

#include "CoreMinimal.h"
#include "AIBase.h"
#include "PlayerAI.generated.h"

class AEnemyAI;

UCLASS()
class RAIDISCOMING_API APlayerAI : public AAIBase
{
    GENERATED_BODY()

public:

    APlayerAI();

    // ������ ����������� �����������
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Detection")
    float DetectionRadius = 35.0f;

    // ����� ���� ����� ������ � �������
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    TArray<AEnemyAI*> FindEnemiesInRadius();

    // ����� ������������� ����������
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    AEnemyAI* SelectTarget(const TArray<AEnemyAI*>& Enemies);

    // ����� � ��������� ����� ����
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void FindTarget();

    // ������� �� ������ ������� ����
    UFUNCTION(BlueprintCallable, Category = "AI|Target")
    void OnTargetDied();

protected:

    virtual void BeginPlay() override;
    void FindTowerTarget(bool bCanFireUnderLighting);
};
