
#include "AI/EnemyMageAI.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "TimerManager.h"

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEnemyMageAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        BarrierTimer,
        this,
        &AEnemyMageAI::UpdateBarrier,
        FMath::Max(BarrierCooldown, 0.01f),
        true
    );
}

void AEnemyMageAI::FindAlliesAhead()
{
    AlliesAhead.Empty();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    TArray<FOverlapResult> Results;

    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    World->OverlapMultiByObjectType(
        Results,
        GetActorLocation(),
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(AllyDetectionRadius),
        QueryParams
    );

    FVector Forward = GetActorForwardVector();
    Forward.Z = 0.0f;
    Forward.Normalize();

    for (const FOverlapResult& Result : Results)
    {
        AEnemyAI* Ally = Cast<AEnemyAI>(Result.GetActor());

        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        FVector Direction =
            Ally->GetActorLocation() - GetActorLocation();

        Direction.Z = 0.0f;

        if (Direction.IsNearlyZero())
        {
            continue;
        }

        const float Dot = FVector::DotProduct(
            Forward,
            Direction.GetSafeNormal()
        );

        if (Dot > 0.0f)
        {
            AlliesAhead.AddUnique(Ally);
        }
    }
}

bool AEnemyMageAI::CanCreateBarrier() const
{
    return IsAlive() && AlliesAhead.Num() > 0;
}

void AEnemyMageAI::Barrier()
{
    if (!CanCreateBarrier())
    {
        return;
    }

    for (AEnemyAI* Ally : AlliesAhead)
    {
        if (!IsValid(Ally) || !Ally->IsAlive())
        {
            continue;
        }

        // TODO:
        // Здесь подключим компонент защитного барьера.
        // Он будет использовать BarrierHP и BarrierDuration.
    }
}

void AEnemyMageAI::UpdateBarrier()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(BarrierTimer);
        return;
    }

    FindAlliesAhead();

    if (CanCreateBarrier())
    {
        Barrier();
    }
}

void AEnemyMageAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(BarrierTimer);

    Super::EndPlay(EndPlayReason);
}
