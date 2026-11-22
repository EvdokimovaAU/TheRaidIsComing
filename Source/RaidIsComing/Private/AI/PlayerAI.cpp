#include "AI/PlayerAI.h"
#include "AI/EnemyTorchbearerAI.h"
#include "AI/EnemyAI.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"

// �����������
APlayerAI::APlayerAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

// ������ ����
void APlayerAI::BeginPlay()
{
    Super::BeginPlay();
}

// ����� ������ � �������
TArray<AEnemyAI*> APlayerAI::FindEnemiesInRadius()
{
    TArray<AEnemyAI*> Enemies;

    if (!GetWorld())
    {
        return Enemies;
    }

    TArray<FOverlapResult> Results;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByObjectType(
        Results,
        GetActorLocation(),
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(DetectionRadius),
        QueryParams
    );

    for (const FOverlapResult& Result : Results)
    {
        AEnemyAI* Enemy = Cast<AEnemyAI>(Result.GetActor());

        if (IsValid(Enemy) && Enemy->IsAlive())
        {
            Enemies.AddUnique(Enemy);
        }
    }

    return Enemies;
}

// ����� ������������ ����
AEnemyAI* APlayerAI::SelectTarget(
    const TArray<AEnemyAI*>& Enemies)
{
    AEnemyAI* BestTarget = nullptr;

    for (AEnemyAI* Enemy : Enemies)
    {
        if (!IsValid(Enemy) || !Enemy->IsAlive())
        {
            continue;
        }

        if (!BestTarget)
        {
            BestTarget = Enemy;
            continue;
        }

        // ��� ������ ����������� �� ��������?
        if (Enemy->RouteProgress > BestTarget->RouteProgress)
        {
            BestTarget = Enemy;
        }
        // ��� ���������� ��������� �������� ������� ID
        else if (
            FMath::IsNearlyEqual(
                Enemy->RouteProgress,
                BestTarget->RouteProgress
            )
            && Enemy->ID < BestTarget->ID
            )
        {
            BestTarget = Enemy;
        }
    }

    return BestTarget;
}

// ����� � ���������� ����
void APlayerAI::FindTarget()
{
    TArray<AEnemyAI*> Enemies = FindEnemiesInRadius();

    Target = SelectTarget(Enemies);
}

// ������� �� ������ ����
void APlayerAI::OnTargetDied()
{
    Target = nullptr;

    FindTarget();
}

void APlayerAI::FindTowerTarget(bool bCanFireUnderLighting)
{
    TArray<AEnemyAI*> Enemies = FindEnemiesInRadius();
    if (!bCanFireUnderLighting)
    {
        Enemies.RemoveAll([](const AEnemyAI* Enemy)
        {
            return !Enemy->IsA<AEnemyTorchbearerAI>();
        });
    }
    Target = SelectTarget(Enemies);
}