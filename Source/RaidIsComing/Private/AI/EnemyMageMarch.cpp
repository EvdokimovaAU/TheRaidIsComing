#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "EnemyCombatNavigation.h"
#include "Construction/RICBuilding.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
bool ProjectMeetingSlot(AEnemyAI* Member, const FVector& Candidate, FVector& Out)
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Member->GetWorld());
    FNavLocation Projected;
    if (!Nav || !Nav->ProjectPointToNavigation(Candidate, Projected, FVector(30.f, 30.f, 180.f)) ||
        FVector::Dist2D(Candidate, Projected.Location) > 30.f ||
        !EnemyCombatNavigation::HasCompletePath(Member, Projected.Location)) return false;
    const auto* Capsule = Member->GetCapsuleComponent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EscortMeeting), false, Member);
    for (TActorIterator<AEnemyAI> It(Member->GetWorld()); It; ++It) Params.AddIgnoredActor(*It);
    const FVector Center = Projected.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
    if (Member->GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
            Capsule->GetScaledCapsuleHalfHeight() - 2.f), Params)) return false;
    Out = Projected.Location;
    return true;
}
}

bool AEnemyMageAI::BindSpawnCompanion(AEnemySwordsmanAI* Ally)
{
    if (!IsValid(Ally) || IsInCapitolRecruitmentZone(this) || IsInCapitolRecruitmentZone(Ally) ||
        !Ally->TryAssignEscortMage(this)) return false;
    EscortGroup.AddUnique(Ally);
    JoinedEscort.Add(Ally);
    FollowedSwordsman = Ally;
    bEscortMarchActive = true;
    bSpawnGroupNeedsBarrier = BarrierClass != nullptr;
    GetCharacterMovement()->MaxWalkSpeed = FMath::Min(NormalMovementSpeed, Ally->MovementSpeed);
    StopMovement();
    Ally->WaitForMage(this);
    WaitingSwordsmen.AddUnique(Ally);
    ClearMarchPlan();
    UE_LOG(LogTemp, Log, TEXT("[Mage] Spawn group bound immediately: %s + %s"), *GetName(), *Ally->GetName());
    return true;
}

FVector AEnemyMageAI::PointOnEscortRoute(float Progress) const
{
    if (EscortRoute.IsEmpty()) return GetActorLocation();
    for (int32 Index = 1; Index < EscortRoute.Num(); ++Index)
    {
        if (Progress > EscortRouteDistances[Index]) continue;
        const float Length = EscortRouteDistances[Index] - EscortRouteDistances[Index - 1];
        return FMath::Lerp(EscortRoute[Index - 1], EscortRoute[Index],
            FMath::Clamp((Progress - EscortRouteDistances[Index - 1]) / FMath::Max(1.f, Length), 0.f, 1.f));
    }
    return EscortRoute.Last();
}

float AEnemyMageAI::ProjectOntoEscortRoute(const FVector& Position, float MinProgress, float MaxProgress) const
{
    float Best = MinProgress, Distance = TNumericLimits<float>::Max();
    for (int32 Index = 1; Index < EscortRoute.Num(); ++Index)
    {
        if (EscortRouteDistances[Index] < MinProgress || EscortRouteDistances[Index - 1] > MaxProgress) continue;
        FVector A = EscortRoute[Index - 1], B = EscortRoute[Index];
        A.Z = B.Z = Position.Z;
        const float Length = FVector::Dist2D(A, B);
        const FVector Closest = FMath::ClosestPointOnSegment(Position, A, B);
        const float Progress = FMath::Clamp(EscortRouteDistances[Index - 1] + FVector::Dist2D(A, Closest),
            MinProgress, FMath::Min(MaxProgress, EscortRouteDistances.Last()));
        const float Score = FVector::DistSquared2D(Position, PointOnEscortRoute(Progress));
        if (Length > 0.f && Score < Distance) { Distance = Score; Best = Progress; }
    }
    return Best;
}

bool AEnemyMageAI::BuildEscortRoute()
{
    if (!EscortRoute.IsEmpty()) return true;
    if (!EnsureMarchAnchor()) return false;
    AEnemyAI* Leader = this;
    if (FollowedSwordsman.IsValid()) Leader = FollowedSwordsman.Get();
    FVector Goal = MarchAnchor;
    if (auto* Sword = Cast<AEnemySwordsmanAI>(Leader))
        if (!FindEscortMarchDestination(Sword, Goal)) return false;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Path = Nav ? Nav->FindPathToLocationSynchronously(GetWorld(), Leader->GetActorLocation(), Goal, Leader) : nullptr;
    if (!IsValid(Path) || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2) return false;
    EscortRoute = Path->PathPoints;
    EscortRouteDistances = {0.f};
    for (int32 Index = 1; Index < EscortRoute.Num(); ++Index)
        EscortRouteDistances.Add(EscortRouteDistances.Last() + FVector::Dist2D(EscortRoute[Index - 1], EscortRoute[Index]));
    MarchMoves.Empty();
    bFinishingEscortMarch = false;
    if (MeetingRecruit.IsValid())
        MeetingProgress = ProjectOntoEscortRoute(MeetingSlots.FindChecked(this), 0.f, EscortRouteDistances.Last());
    UE_LOG(LogTemp, Log, TEXT("[Mage] Shared corridor built: %d points, length %.1f, leader %s"),
        EscortRoute.Num(), EscortRouteDistances.Last(), *GetNameSafe(Leader));
    return true;
}

void AEnemyMageAI::IssueEscortMove(AEnemyAI* Member, const FVector& Goal, float Speed)
{
    if (!IsValid(Member) || !Member->IsAlive()) return;
    auto* AI = Cast<AAIController>(Member->GetController());
    if (!AI) return;
    if (auto* Sword = Cast<AEnemySwordsmanAI>(Member))
        Sword->SetEscortSpeedLimit(this, Speed, 10.f);
    else Member->GetCharacterMovement()->MaxWalkSpeed = Speed;
    auto& Move = MarchMoves.FindOrAdd(Member);
    const bool bSameGoal = Move.bIssued && FVector::Dist2D(Move.Goal, Goal) < FMath::Max(30.f, RepathDistance);
    if (bSameGoal && AI->GetMoveStatus() == EPathFollowingStatus::Moving) return;
    if (FVector::Dist2D(Member->GetActorLocation(), Goal) <= 10.f) return;
    // Full paths to corridor points only. Replacing an advancing waypoint does
    // not call StopMovement or zero character velocity.
    if (!EnemyCombatNavigation::HasCompletePath(Member, Goal)) return;
    Move.Goal = Goal;
    Move.bIssued = true;
    AI->MoveToLocation(Goal, EnemyCombatNavigation::AcceptanceRadius, false, true, false, false, nullptr, false);
}

void AEnemyMageAI::UpdateEscortMarch()
{
    if (JoinedEscort.IsEmpty() || bCastingBarrier || bAttacking || ObstaclePhase != EObstaclePhase::None) return;
    bEscortMarchActive = true;
    if (!BuildEscortRoute()) return;
    if (UpdateRecruitMeeting()) return;
    auto* Leader = FollowedSwordsman.Get();
    if (!IsValid(Leader) || !Leader->IsAlive()) return;
    TArray<AEnemyAI*> Members{Leader, this};
    for (const auto& Weak : EscortGroup)
        if (Weak.IsValid() && Weak.Get() != Leader && JoinedEscort.Contains(Weak)) Members.Add(Weak.Get());
    float Speed = NormalMovementSpeed;
    float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    for (auto* Member : Members)
    {
        Speed = FMath::Min(Speed, Member->MovementSpeed);
        CapsuleRadius = FMath::Max(CapsuleRadius, Member->GetCapsuleComponent()->GetScaledCapsuleRadius());
        auto& Move = MarchMoves.FindOrAdd(Member);
        // A bounded cursor prevents a hairpin's neighbouring lane from being
        // mistaken for a much later section of the route.
        Move.Progress = ProjectOntoEscortRoute(Member->GetActorLocation(),
            FMath::Max(0.f, Move.Progress - 40.f), Move.Progress + 200.f);
    }
    const float Spacing = CapsuleRadius * 2.f + 12.f;
    const float LeadProgress = MarchMoves.FindChecked(Leader).Progress;
    bFinishingEscortMarch |= EscortRouteDistances.Last() - LeadProgress <= Spacing;
    if (bFinishingEscortMarch)
    {
        IssueEscortMove(this, MarchAnchor, Speed);
        for (auto* Member : Members)
            if (auto* Sword = Cast<AEnemySwordsmanAI>(Member))
            {
                FVector Goal;
                if (FindEscortMarchDestination(Sword, Goal)) IssueEscortMove(Sword, Goal, Speed);
            }
        return;
    }
    float LeadSpeed = Speed;
    for (int32 Index = 1; Index < Members.Num(); ++Index)
    {
        const float GapError = LeadProgress - MarchMoves.FindChecked(Members[Index]).Progress - Spacing * Index;
        LeadSpeed = FMath::Min(LeadSpeed, Speed - FMath::Max(0.f, GapError) * 1.5f);
    }
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        auto* Member = Members[Index];
        const float Desired = LeadProgress - Spacing * Index;
        const float Error = Desired - MarchMoves.FindChecked(Member).Progress;
        const float MemberSpeed = FMath::Clamp(Index == 0 ? LeadSpeed : Speed + Error * 1.5f, Speed * 0.15f, Speed);
        const float GoalProgress = FMath::Max(MarchMoves.FindChecked(Member).Progress + 80.f, Desired + 180.f);
        IssueEscortMove(Member, PointOnEscortRoute(GoalProgress), MemberSpeed);
    }
}

void AEnemyMageAI::CancelRecruitMeeting(const TCHAR* Reason)
{
    if (auto* Recruit = MeetingRecruit.Get())
    {
        if (!JoinedEscort.Contains(Recruit))
        {
            UE_LOG(LogTemp, Log, TEXT("[Mage] Cancel meeting with %s: %s"), *Recruit->GetName(), Reason);
            WaitingSwordsmen.Remove(Recruit);
            EscortGroup.Remove(Recruit);
            Recruit->ReleaseEscortMage(this);
        }
    }
    MeetingRecruit.Reset();
    MeetingSlots.Empty();
    bApproachingMeeting = false;
    bRecruitBarrierRefreshPending = false;
}

bool AEnemyMageAI::ReserveRecruit(AEnemySwordsmanAI* Ally)
{
    if (IsInCapitolRecruitmentZone(this) || IsInCapitolRecruitmentZone(Ally) ||
        !BuildEscortRoute() || MeetingRecruit.IsValid() || !IsAllyOnRoute(Ally)) return false;
    const float LeadProgress = ProjectOntoEscortRoute(GetActorLocation(), 0.f, EscortRouteDistances.Last());
    const float Progress = ProjectOntoEscortRoute(Ally->GetActorLocation(), LeadProgress, EscortRouteDistances.Last());
    const float Radius = FMath::Max(GetCapsuleComponent()->GetScaledCapsuleRadius(),
        Ally->GetCapsuleComponent()->GetScaledCapsuleRadius());
    const float Spacing = Radius * 2.f + 12.f;
    for (int32 Attempt = 0; Attempt < 3; ++Attempt)
    {
        const float At = FMath::Min(Progress + Attempt * Spacing, EscortRouteDistances.Last());
        const FVector Center = PointOnEscortRoute(At);
        const FVector Direction = (PointOnEscortRoute(At + 10.f) - PointOnEscortRoute(FMath::Max(0.f, At - 10.f))).GetSafeNormal2D();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Direction);
        TMap<TWeakObjectPtr<AEnemyAI>, FVector> Slots;
        FVector Slot;
        if (!ProjectMeetingSlot(this, Center, Slot)) continue;
        if (EnemyCombatNavigation::SurfaceDistance(Slot + FVector(0.f, 0.f,
            GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Cast<ARICBuilding>(Capitol.Get())) <=
            CapitolRecruitmentExclusionDistance) continue;
        Slots.Add(this, Slot);
        bool bValid = true;
        for (const auto& Weak : EscortGroup)
            if (Weak.IsValid() && JoinedEscort.Contains(Weak))
            {
                if (!ProjectMeetingSlot(Weak.Get(), Center - Right * Spacing, Slot)) { bValid = false; break; }
                Slots.Add(Weak.Get(), Slot);
            }
        if (!bValid || !ProjectMeetingSlot(Ally, Center + Right * Spacing, Slot)) continue;
        Slots.Add(Ally, Slot);
        TArray<FVector> Positions;
        Slots.GenerateValueArray(Positions);
        for (int32 A = 0; A < Positions.Num(); ++A)
            for (int32 B = A + 1; B < Positions.Num(); ++B)
                bValid &= FVector::Dist2D(Positions[A], Positions[B]) >= Radius * 2.f + 4.f;
        if (!bValid || !Ally->TryAssignEscortMage(this)) continue;
        MeetingRecruit = Ally;
        MeetingSlots = MoveTemp(Slots);
        MeetingProgress = At;
        bApproachingMeeting = false;
        bRecruitBarrierRefreshPending = true;
        EscortGroup.AddUnique(Ally);
        Ally->WaitForMage(this);
        WaitingSwordsmen.AddUnique(Ally);
        UE_LOG(LogTemp, Log, TEXT("[Mage] Meeting reserved for %s on shared route at %s"), *Ally->GetName(), *Center.ToString());
        return true;
    }
    return false;
}

bool AEnemyMageAI::UpdateRecruitMeeting()
{
    auto* Recruit = MeetingRecruit.Get();
    if (!IsValid(Recruit)) return false;
    if (IsInCapitolRecruitmentZone(this) || IsInCapitolRecruitmentZone(Recruit))
    { CancelRecruitMeeting(TEXT("Capitol recruitment exclusion zone")); return false; }
    if (!IsAllyOnRoute(Recruit)) { CancelRecruitMeeting(TEXT("route no longer passes rendezvous")); return false; }
    const float Lead = FollowedSwordsman.IsValid()
        ? ProjectOntoEscortRoute(FollowedSwordsman->GetActorLocation(), 0.f, EscortRouteDistances.Last())
        : ProjectOntoEscortRoute(GetActorLocation(), 0.f, EscortRouteDistances.Last());
    IssueEscortMove(Recruit, MeetingSlots.FindChecked(Recruit), Recruit->MovementSpeed);
    bApproachingMeeting |= MeetingProgress - Lead <= 180.f;
    if (!bApproachingMeeting) return false;
    bool bReady = true;
    float Speed = NormalMovementSpeed;
    for (const auto& Weak : EscortGroup) if (Weak.IsValid()) Speed = FMath::Min(Speed, Weak->MovementSpeed);
    for (const auto& Entry : MeetingSlots)
    {
        auto* Member = Entry.Key.Get();
        if (!IsValid(Member) || !Member->IsAlive()) { CancelRecruitMeeting(TEXT("member disappeared")); return false; }
        if (!EnemyCombatNavigation::HasCompletePath(Member, Entry.Value))
        { CancelRecruitMeeting(TEXT("meeting position became unreachable")); return false; }
        IssueEscortMove(Member, Entry.Value, Speed);
        bReady &= FVector::Dist2D(Member->GetActorLocation(), Entry.Value) <= 12.f;
    }
    if (!bReady) return true;
    JoinedEscort.Add(Recruit);
    MeetingRecruit.Reset();
    MeetingSlots.Empty();
    bApproachingMeeting = false;
    bRecruitBarrierRefreshPending = false;
    bRecruitBarrierRefreshCasting = true;
    for (const auto& Weak : EscortGroup) if (Weak.IsValid()) Weak->StopMovement();
    StopMovement();
    StopNearbySwordsmen();
    ClearMarchPlan();
    UE_LOG(LogTemp, Log, TEXT("[Mage] Meeting assembled; replacing dome for %d swordsmen"), JoinedEscort.Num());
    if (HasActiveBarrier() && !BeginBarrierRemoval()) return true;
    if (CanCreateBarrier()) Barrier();
    return true;
}
