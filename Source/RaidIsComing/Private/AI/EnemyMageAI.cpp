
#include "AI/EnemyMageAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemySwordsmanAI.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemyMageAI::AEnemyMageAI()
{
    PrimaryActorTick.bCanEverTick = false;
    bAutoStartMovement = false;

    MovementSpeed = 300.0f;
    AcceptanceRadius = 100.0f;
}

void AEnemyMageAI::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* Movement =
        GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = NormalMovementSpeed;
        Movement->MaxAcceleration = MageAcceleration;
        Movement->BrakingDecelerationWalking =
            MageBrakingDeceleration;
        Movement->GroundFriction = 4.0f;
        Movement->bRequestedMoveUseAcceleration = true;
    }

    CurrentDesiredSpeed = NormalMovementSpeed;

    const float Interval =
        FMath::Max(0.05f, FollowUpdateInterval);

    GetWorldTimerManager().SetTimer(
        BehaviorTimer,
        this,
        &AEnemyMageAI::UpdateBehavior,
        Interval,
        true
    );

    UpdateBehavior();
}

bool AEnemyMageAI::IsSwordsman(
    const AEnemyAI* Enemy) const
{
    return IsValid(Enemy) &&
        Enemy->IsA(AEnemySwordsmanAI::StaticClass());
}

void AEnemyMageAI::FindAlliesAhead()
{
    AlliesAhead.Reset();

    if (!GetWorld())
        return;

    FVector Forward = GetActorForwardVector();

    if (IsValid(Capitol))
    {
        Forward = Capitol->GetActorLocation() -
            GetActorLocation();
    }

    Forward.Z = 0.0f;

    if (!Forward.Normalize())
        return;

    for (TActorIterator<AEnemySwordsmanAI>
        It(GetWorld()); It; ++It)
    {
        AEnemySwordsmanAI* Ally = *It;

        if (!IsValid(Ally) || !Ally->IsAlive())
            continue;

        FVector Offset =
            Ally->GetActorLocation() -
            GetActorLocation();

        Offset.Z = 0.0f;

        if (Offset.SizeSquared2D() >
            FMath::Square(AllyDetectionRadius))
            continue;

        if (Offset.IsNearlyZero())
            continue;

        if (FVector::DotProduct(
            Forward,
            Offset.GetSafeNormal()) > 0.0f)
        {
            AlliesAhead.Add(Ally);
        }
    }
}

void AEnemyMageAI::SelectSwordsman()
{
    AEnemyAI* BestAlly = nullptr;
    float BestDistance = TNumericLimits<float>::Max();

    for (AEnemyAI* Ally : AlliesAhead)
    {
        if (!IsSwordsman(Ally) || !Ally->IsAlive())
            continue;

        const float Distance =
            FVector::DistSquared2D(
                GetActorLocation(),
                Ally->GetActorLocation());

        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            BestAlly = Ally;
        }
    }

    // Меняем цель только при необходимости.
    if (FollowedSwordsman.Get() != BestAlly)
    {
        FollowedSwordsman = BestAlly;
        bFollowingSwordsman = false;
        bMovingToCapitol = false;

        UE_LOG(LogTemp, Log,
            TEXT("[Mage] Target changed: %s"),
            IsValid(BestAlly)
            ? *BestAlly->GetName()
            : TEXT("None"));
    }
}

bool AEnemyMageAI::HasActiveBarrier() const
{
    return IsValid(ActiveBarrier) &&
        ActiveBarrier->IsBarrierActive();
}

bool AEnemyMageAI::CanCreateBarrier() const
{
    return IsAlive() &&
        !HasActiveBarrier() &&
        BarrierClass != nullptr &&
        GetWorld() &&
        GetWorld()->GetTimeSeconds() >= NextBarrierTime;
}

void AEnemyMageAI::Barrier()
{
    if (!CanCreateBarrier())
        return;

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveBarrier =
        GetWorld()->SpawnActor<AEnemyBarrier>(
            BarrierClass,
            GetActorLocation(),
            FRotator::ZeroRotator,
            Params
        );

    if (!IsValid(ActiveBarrier))
    {
        NextBarrierTime =
            GetWorld()->GetTimeSeconds() +
            BarrierCooldown;
        return;
    }

    ActiveBarrier->ActivateBarrier(
        BarrierHP,
        BarrierDuration
    );

    ActiveBarrier->AttachToMage(this);

    NextBarrierTime =
        GetWorld()->GetTimeSeconds() +
        BarrierCooldown;

    UE_LOG(LogTemp, Log,
        TEXT("[Mage] Barrier created"));
}

void AEnemyMageAI::RemoveBarrier()
{
    if (IsValid(ActiveBarrier))
    {
        ActiveBarrier->RemoveBarrier();
        ActiveBarrier->Destroy();
    }

    ActiveBarrier = nullptr;
}

void AEnemyMageAI::UpdateFollowSpeed(float DeltaTime)
{
    UCharacterMovementComponent* Movement =
        GetCharacterMovement();

    if (!Movement)
        return;

    float TargetSpeed = NormalMovementSpeed;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    if (IsValid(Ally) && Ally->IsAlive())
    {
        const float Distance =
            FVector::Dist2D(
                GetActorLocation(),
                Ally->GetActorLocation());

        const float AllySpeed =
            Ally->GetVelocity().Size2D();

        if (Distance > FollowDistance + 100.0f)
        {
            // Маг отстал — ускоряемся.
            TargetSpeed = MageMovementSpeed;
        }
        else if (Distance > FollowDistance)
        {
            TargetSpeed = FMath::Clamp(
                AllySpeed + 30.0f,
                NormalMovementSpeed,
                MageMovementSpeed);
        }
        else
        {
            // Маг рядом — подстраивается.
            TargetSpeed = FMath::Clamp(
                AllySpeed,
                NormalMovementSpeed,
                MageMovementSpeed);
        }
    }

    CurrentDesiredSpeed = FMath::FInterpConstantTo(
        CurrentDesiredSpeed,
        TargetSpeed,
        DeltaTime,
        SpeedChangeRate);

    Movement->MaxWalkSpeed = CurrentDesiredSpeed;
}

void AEnemyMageAI::UpdateMovement()
{
    if (!IsAlive() || !IsValid(Capitol))
        return;

    AAIController* MageController =
        Cast<AAIController>(GetController());

    if (!MageController)
        return;

    AEnemyAI* Ally = FollowedSwordsman.Get();

    if (IsValid(Ally) && Ally->IsAlive())
    {
        MageState = EMageState::FollowingSwordsmen;

        if (!bFollowingSwordsman ||
            CurrentMoveTarget.Get() != Ally)
        {
            CurrentMoveTarget = Ally;
            bFollowingSwordsman = true;
            bMovingToCapitol = false;

            // Маленький радиус, чтобы маг
            // не останавливался далеко от мечника.
            MageController->MoveToActor(
                Ally,
                35.0f,
                true,
                true,
                true
            );

            UE_LOG(LogTemp, Log,
                TEXT("[Mage] Following swordsman: %s"),
                *Ally->GetName());
        }

        return;
    }

    MageState = HasActiveBarrier()
        ? EMageState::MovingAlone
        : EMageState::WaitingForBarrier;

    if (!bMovingToCapitol ||
        CurrentMoveTarget.Get() != Capitol.Get())
    {
        CurrentMoveTarget = Capitol.Get();
        bMovingToCapitol = true;
        bFollowingSwordsman = false;

        MageController->MoveToActor(
            Capitol.Get(),
            AcceptanceRadius,
            true,
            true,
            true
        );

        UE_LOG(LogTemp, Log,
            TEXT("[Mage] Moving to Capitol"));
    }
}

void AEnemyMageAI::MoveToTarget()
{
    UpdateMovement();
}

void AEnemyMageAI::UpdateBehavior()
{
    if (!IsAlive())
    {
        GetWorldTimerManager().ClearTimer(
            BehaviorTimer);
        return;
    }

    FindAlliesAhead();
    SelectSwordsman();

    if (IsValid(ActiveBarrier) &&
        !ActiveBarrier->IsBarrierActive())
    {
        RemoveBarrier();
    }

    if (!HasActiveBarrier() && CanCreateBarrier())
    {
        Barrier();
    }

    const float Interval =
        FMath::Max(0.05f, FollowUpdateInterval);

    UpdateFollowSpeed(Interval);
    UpdateMovement();
}

void AEnemyMageAI::Die()
{
    if (!IsAlive())
        return;

    GetWorldTimerManager().ClearTimer(BehaviorTimer);
    RemoveBarrier();

    Super::Die();
}

void AEnemyMageAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(BehaviorTimer);
    RemoveBarrier();

    Super::EndPlay(EndPlayReason);
}
