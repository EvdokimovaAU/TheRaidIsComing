
#include "AI/AIBase.h"
#include "Engine/World.h"

AAIBase::AAIBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AAIBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;
    bIsDead = false;
}

void AAIBase::ApplyCombatBalance(
    float NewMaxHP,
    float NewDamage,
    float NewAttackRange,
    float NewAttackCooldown)
{
    const float OldMaxHP = MaxHP;

    MaxHP = FMath::Max(1.0f, NewMaxHP);
    Damage = FMath::Max(0.0f, NewDamage);
    AttackRange = FMath::Max(0.0f, NewAttackRange);
    AttackCooldown = FMath::Max(0.01f, NewAttackCooldown);

    // Preserve the current health percentage if called
    // after BeginPlay.
    if (HasActorBegunPlay() && !bIsDead)
    {
        const float HealthRatio =
            OldMaxHP > 0.0f
            ? FMath::Clamp(CurrentHP / OldMaxHP, 0.0f, 1.0f)
            : 1.0f;

        CurrentHP = MaxHP * HealthRatio;
    }

    UE_LOG(LogTemp, Log,
        TEXT("[AIBase] %s balance applied: HP=%.1f Damage=%.1f Range=%.1f Cooldown=%.2f"),
        *GetName(),
        MaxHP,
        Damage,
        AttackRange,
        AttackCooldown);
}

void AAIBase::ReceiveDamage(float Amount)
{
    if (bIsDead || Amount <= 0.0f)
    {
        return;
    }

    CurrentHP = FMath::Max(CurrentHP - Amount, 0.0f);

    if (CurrentHP <= 0.0f)
    {
        Die();
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[AI] %s | Current HP: %.1f"),
        *GetName(),
        CurrentHP);
}

bool AAIBase::IsAlive() const
{
    return !bIsDead && CurrentHP > 0.0f;
}

bool AAIBase::IsTargetValid() const
{
    return IsValid(Target.Get()) && Target->IsAlive();
}

bool AAIBase::CanAttack() const
{
    if (!IsAlive() || !IsTargetValid() || !GetWorld())
    {
        return false;
    }

    const float Distance = FVector::Dist(
        GetActorLocation(),
        Target->GetActorLocation()
    );

    if (Distance > AttackRange)
    {
        return false;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    return CurrentTime - LastAttackTime >= AttackCooldown;
}

void AAIBase::Attack()
{
    if (!CanAttack())
    {
        return;
    }

    LastAttackTime = GetWorld()->GetTimeSeconds();

    Target->ReceiveDamage(Damage);
}

void AAIBase::Die()
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;
    CurrentHP = 0.0f;

    Target = nullptr;
}
