#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "Construction/RICBuilding.h"
#include "EnemyCombatNavigation.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

bool AEnemyMageAI::IsInCapitolRecruitmentZone(const AEnemyAI* Enemy) const
{
    const auto* Building = Cast<ARICBuilding>(Capitol.Get());
    return IsValid(Enemy) && IsValid(Building) &&
        EnemyCombatNavigation::SurfaceDistance(Enemy->GetActorLocation(), Building) <=
            FMath::Max(0.f, CapitolRecruitmentExclusionDistance);
}

bool AEnemyMageAI::BeginBarrierRemoval()
{
    if (bRemovingBarrier) return false;
    if (!HasActiveBarrier()) return true;
    CancelBarrierCast();
    bRemovingBarrier = true;
    StopMovement();
    GetCharacterMovement()->StopMovementImmediately();
    ResetMovementRequest();
    for (const auto& Weak : EscortGroup)
        if (Weak.IsValid() && Weak->IsAlive())
        {
            Weak->StopMovement();
            Weak->GetCharacterMovement()->StopMovementImmediately();
        }
    AActor* FacingTarget = Capitol.Get();
    if (CombatTarget.IsValid()) FacingTarget = CombatTarget.Get();
    if (EscortStagingWall.IsValid()) FacingTarget = EscortStagingWall.Get();
    EnemyCombatNavigation::FaceTarget(this, FacingTarget);

    UAnimMontage* Montage = BarrierRemovalMontage ? BarrierRemovalMontage.Get() : BarrierMontage.Get();
    const bool bReverseCast = !BarrierRemovalMontage;
    float Duration = 0.f;
    if (Montage && GetMesh())
        if (auto* Anim = GetMesh()->GetAnimInstance())
        {
            const float Rate = bReverseCast ? -1.f : 1.f;
            const float Start = bReverseCast ? FMath::Max(0.f, Montage->GetPlayLength() - KINDA_SMALL_NUMBER) : 0.f;
            // MontageLength stays positive for reverse playback. Duration's
            // return mode divides by a negative rate and cannot feed a timer.
            const float Length = Anim->Montage_Play(Montage, Rate,
                EMontagePlayReturnType::MontageLength, Start);
            if (Length > 0.f)
            {
                PlayingBarrierRemovalMontage = Montage;
                Duration = Length / FMath::Max(0.01f, FMath::Abs(Rate * Montage->RateScale));
            }
        }
    UE_LOG(LogTemp, Log, TEXT("[Mage] Barrier removal animation: %s, reverse=%d, duration=%.2f"),
        *GetNameSafe(Montage), bReverseCast, Duration);
    if (Duration <= 0.f)
    {
        FinishBarrierRemoval();
        return true;
    }
    GetWorldTimerManager().SetTimer(BarrierRemovalTimer, this,
        &AEnemyMageAI::FinishBarrierRemoval, Duration, false);
    return false;
}

void AEnemyMageAI::FinishBarrierRemoval()
{
    if (!bRemovingBarrier) return;
    GetWorldTimerManager().ClearTimer(BarrierRemovalTimer);
    bRemovingBarrier = false;
    PlayingBarrierRemovalMontage.Reset();
    RemoveBarrierInternal(false);
    UE_LOG(LogTemp, Log, TEXT("[Mage] Barrier removal finished; group action may continue"));
}

void AEnemyMageAI::CancelBarrierRemoval()
{
    GetWorldTimerManager().ClearTimer(BarrierRemovalTimer);
    if (auto* Montage = PlayingBarrierRemovalMontage.Get())
        if (GetMesh())
            if (auto* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.1f, Montage);
    PlayingBarrierRemovalMontage.Reset();
    bRemovingBarrier = false;
}
