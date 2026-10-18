
#include "AIBase.h"
#include "Engine/World.h"

// Конструктор
AAIBase::AAIBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

// Начало игры
void AAIBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;
    bIsDead = false;
}

// Получение урона
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

// Проверка жизни
bool AAIBase::IsAlive() const
{
    return !bIsDead && CurrentHP > 0.0f;
}

// Проверка цели
bool AAIBase::IsTargetValid() const
{
    return IsValid(Target.Get()) && Target->IsAlive();
}

// Проверка возможности атаки
bool AAIBase::CanAttack() const
{
    if (!IsAlive() || !IsTargetValid())
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

// Базовая атака
void AAIBase::Attack()
{
    if (!CanAttack())
    {
        return;
    }

    LastAttackTime = GetWorld()->GetTimeSeconds();

    Target->ReceiveDamage(Damage);
}

// Смерть
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
