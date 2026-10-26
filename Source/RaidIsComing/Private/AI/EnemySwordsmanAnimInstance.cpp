
#include "AI/EnemySwordsmanAnimInstance.h"
#include "AI/EnemySwordsmanAI.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"

void UEnemySwordsmanAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerPawn = TryGetPawnOwner();
}

void UEnemySwordsmanAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!IsValid(OwnerPawn))
    {
        OwnerPawn = TryGetPawnOwner();
    }

    if (!IsValid(OwnerPawn))
    {
        Speed = 0.0f;
        bIsMoving = false;
        return;
    }

    AEnemySwordsmanAI* Swordsman =
        Cast<AEnemySwordsmanAI>(OwnerPawn);

    if (IsValid(Swordsman))
    {
        bIsDead = !Swordsman->IsAlive();
    }

    if (bIsDead)
    {
        Speed = 0.0f;
        bIsMoving = false;
        bIsAttacking = false;
        return;
    }

    Speed = OwnerPawn->GetVelocity().Size2D();

    bIsMoving = Speed > 3.0f;
}

void UEnemySwordsmanAnimInstance::PlayAttackAnimation()
{
    if (bIsDead)
    {
        return;
    }

    if (!AttackMontage)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAnim] AttackMontage is not assigned!")
        );

        return;
    }

    if (Montage_IsPlaying(AttackMontage))
    {
        return;
    }

    bIsAttacking = true;

    const float Duration = Montage_Play(AttackMontage);

    if (Duration <= 0.0f)
    {
        bIsAttacking = false;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[EnemyAnim] Failed to play attack montage")
        );
    }
}

void UEnemySwordsmanAnimInstance::StopAttackAnimation()
{
    if (AttackMontage && Montage_IsPlaying(AttackMontage))
    {
        Montage_Stop(0.0f, AttackMontage);
    }

    bIsAttacking = false;
}

void UEnemySwordsmanAnimInstance::SetAttacking(bool bNewAttacking)
{
    if (bIsDead)
    {
        bIsAttacking = false;
        return;
    }

    bIsAttacking = bNewAttacking;
}
