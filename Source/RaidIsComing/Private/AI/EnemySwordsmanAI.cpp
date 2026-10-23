
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemySwordsmanAnimInstance.h"

#include "Engine/DamageEvents.h"
#include "AIController.h"
#include "Construction/RICBuilding.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "Components/SkeletalMeshComponent.h"

AEnemySwordsmanAI::AEnemySwordsmanAI()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEnemySwordsmanAI::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        CapitolCheckTimer,
        this,
        &AEnemySwordsmanAI::CheckCapitolDistance,
        0.2f,
        true
    );
}

// Check distance to the Capitol
void AEnemySwordsmanAI::CheckCapitolDistance()
{
    if (!IsAlive() || bAttackingCapitol || IsValid(TargetWall))
    {
        return;
    }

    ARICBuilding* Building = Cast<ARICBuilding>(Capitol.Get());

    if (!IsValid(Building) || !Building->IsAlive())
    {
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()
    );

    if (Distance <= AttackRange)
    {
        StartCapitolAttack();
    }
}

// Start attacking the Capitol
void AEnemySwordsmanAI::StartCapitolAttack()
{
    ARICBuilding* Building = Cast<ARICBuilding>(Capitol.Get());

    if (bAttackingCapitol ||
        !IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive())
    {
        return;
    }

    bAttackingCapitol = true;

    StopMovement();

    UE_LOG(LogTemp, Warning,
        TEXT("[Swordsman] Reached Capitol. Attack started!"));

    GetWorldTimerManager().SetTimer(
        CapitolAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackCapitol,
        FMath::Max(AttackCooldown, 0.01f),
        true
    );
}

// One attack
void AEnemySwordsmanAI::AttackCapitol()
{
    ARICBuilding* Building = Cast<ARICBuilding>(Capitol.Get());

    if (!IsAlive() ||
        !IsValid(Building) ||
        !Building->IsAlive())
    {
        StopCapitolAttack();
        return;
    }

    const float Distance = FVector::Dist2D(
        GetActorLocation(),
        Building->GetActorLocation()
    );

    if (Distance > AttackRange)
    {
        StopCapitolAttack();
        MoveToTarget();
        return;
    }

    // Play attack animation
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UEnemySwordsmanAnimInstance* AnimInstance =
            Cast<UEnemySwordsmanAnimInstance>(
                MeshComponent->GetAnimInstance()))
        {
            AnimInstance->PlayAttackAnimation();
        }
    }

    // Apply damage
    FDamageEvent DamageEvent(
        UDamageType::StaticClass()
    );

    const float AppliedDamage = Building->TakeDamage(
        Damage,
        DamageEvent,
        GetController(),
        this
    );

    UE_LOG(LogTemp, Warning,
        TEXT("[Swordsman] Capitol hit! Damage: %.1f"),
        AppliedDamage
    );

    if (!IsValid(Building) || !Building->IsAlive())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Swordsman] Capitol destroyed!"));

        StopCapitolAttack();
    }
}

void AEnemySwordsmanAI::StopCapitolAttack()
{
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);

    bAttackingCapitol = false;

    // Stop attack animation
    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        if (UEnemySwordsmanAnimInstance* AnimInstance =
            Cast<UEnemySwordsmanAnimInstance>(
                MeshComponent->GetAnimInstance()))
        {
            AnimInstance->StopAttackAnimation();
        }
    }
}

// Existing wall logic
void AEnemySwordsmanAI::OnObstacleDetected(AActor* Obstacle)
{
    if (!IsValid(Obstacle))
    {
        return;
    }

    StopCapitolAttack();
    StopMovement();
    StartWallAttack(Obstacle);
}

void AEnemySwordsmanAI::StartWallAttack(AActor* Wall)
{
    if (!IsValid(Wall))
    {
        return;
    }

    StopWallAttack();

    TargetWall = Wall;

    GetWorldTimerManager().SetTimer(
        WallAttackTimer,
        this,
        &AEnemySwordsmanAI::AttackWall,
        FMath::Max(WallAttackCooldown, 0.01f),
        true
    );
}

void AEnemySwordsmanAI::AttackWall()
{
    if (!IsValid(TargetWall))
    {
        OnWallDestroyed();
        return;
    }

    // Wall damage integration is a separate step.
}

void AEnemySwordsmanAI::StopWallAttack()
{
    GetWorldTimerManager().ClearTimer(WallAttackTimer);

    TargetWall = nullptr;
}

void AEnemySwordsmanAI::OnWallDestroyed()
{
    StopWallAttack();
    MoveToTarget();
}

void AEnemySwordsmanAI::Die()
{
    StopCapitolAttack();
    StopWallAttack();

    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);

    Super::Die();
}

void AEnemySwordsmanAI::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(CapitolCheckTimer);
    GetWorldTimerManager().ClearTimer(CapitolAttackTimer);
    GetWorldTimerManager().ClearTimer(WallAttackTimer);

    Super::EndPlay(EndPlayReason);
}
