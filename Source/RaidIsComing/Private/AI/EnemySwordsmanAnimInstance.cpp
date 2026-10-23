
#include "AI/EnemySwordsmanAnimInstance.h"

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

    // Get horizontal movement speed
    Speed = OwnerPawn->GetVelocity().Size2D();

    // Ignore very small velocity fluctuations
    bIsMoving = Speed > 3.0f;
}

void UEnemySwordsmanAnimInstance::PlayAttackAnimation()
{
    if (!AttackMontage)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAnim] AttackMontage is not assigned!"));

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

        UE_LOG(LogTemp, Warning,
            TEXT("[EnemyAnim] Failed to play attack montage"));
    }
}

void UEnemySwordsmanAnimInstance::StopAttackAnimation()
{
    if (AttackMontage && Montage_IsPlaying(AttackMontage))
    {
        Montage_Stop(0.2f, AttackMontage);
    }

    bIsAttacking = false;
}

void UEnemySwordsmanAnimInstance::SetAttacking(bool bNewAttacking)
{
    bIsAttacking = bNewAttacking;
}
