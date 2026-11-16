#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/WaveManager.h"
#include "AI/EnemyCombatNavigation.h"
#include "Construction/RICBuilding.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

class FObserveLevelOneCommand : public IAutomationLatentCommand
{
public:
    explicit FObserveLevelOneCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!World) return false;
        if (Start < 0.f)
        {
            Start = World->GetTimeSeconds();
            for (TActorIterator<ARICBuilding> It(World); It; ++It)
                if (It->BuildingType == ERICBuildingType::Capitol) It->CurrentHP = It->MaxHP = 100000.f;
            for (TActorIterator<AWaveManager> It(World); It; ++It)
            {
                int32 PairWave = INDEX_NONE;
                for (int32 Index = 0; Index < It->Waves.Num() && PairWave == INDEX_NONE; ++Index)
                    for (const auto& Group : It->Waves[Index].SpawnGroups)
                        if (IsValid(Group.SpawnPoint.Get()) && Group.SwordsmanCount > 0 && Group.MageCount > 0)
                        { PairWave = Index; break; }
                // Balance/wave assets are user-editable. Test the first real
                // paired wave rather than assuming wave one always has a mage.
                if (PairWave == INDEX_NONE)
                {
                    // Добавляем пару во временную PIE-копию волны.
                    for (int32 Index = 0; Index < It->Waves.Num() && PairWave == INDEX_NONE; ++Index)
                        for (auto& Group : It->Waves[Index].SpawnGroups)
                            if (IsValid(Group.SpawnPoint.Get()))
                            {
                                Group.SwordsmanCount = FMath::Max(1, Group.SwordsmanCount);
                                Group.MageCount = FMath::Max(1, Group.MageCount);
                                PairWave = Index;
                                Test->AddInfo(TEXT("Added escort pair to test wave"));
                                break;
                            }
                    if (PairWave == INDEX_NONE)
                    {
                        Test->AddError(TEXT("Level_1 has no configured spawn point for escort scenario"));
                        return true;
                    }
                }
                Test->AddInfo(FString::Printf(TEXT("Level_1 escort scenario uses configured wave %d"), PairWave + 1));
                It->CurrentWaveIndex = PairWave - 1;
                It->StartNextWave();
                break;
            }
        }
        if (!Mage.IsValid())
        {
            for (TActorIterator<AEnemyMageAI> It(World); It; ++It)
            {
                Mage = *It;
                Mage->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                if (IsValid(Mage->Capitol))
                    Test->TestTrue(TEXT("Spawned mage faces Capitol"), FVector::DotProduct(Mage->GetActorForwardVector(),
                        (Mage->Capitol->GetActorLocation() - Mage->GetActorLocation()).GetSafeNormal2D()) > 0.99f);
                Original = It->FollowedSwordsman;
                Test->TestTrue(TEXT("Level_1 spawn pair bound before first behavior tick"), Original.IsValid());
                if (Original.IsValid() && IsValid(Mage->Capitol))
                    Test->TestTrue(TEXT("Spawned swordsman faces Capitol"), FVector::DotProduct(Original->GetActorForwardVector(),
                        (Mage->Capitol->GetActorLocation() - Original->GetActorLocation()).GetSafeNormal2D()) > 0.99f);
                if (Original.IsValid()) { OriginalStart = Original->GetActorLocation(); LastPosition = OriginalStart; }
                break;
            }
        }
        const float Now = World->GetTimeSeconds();
        if (Mage.IsValid() && Original.IsValid())
        {
            bLostOriginal |= !Original->IsAssignedToMage(Mage.Get());
            bMageAttacked |= Mage->bAttacking;
            if (!bLoggedApproach && Mage->HasActiveBarrier() && !Mage->bCastingBarrier)
            {
                bLoggedApproach = true;
                FVector MageGoal, SwordGoal, BoundsCenter, BoundsExtent;
                auto* Building = Cast<ARICBuilding>(Mage->Capitol.Get());
                const bool bMageGoal = EnemyCombatNavigation::FindApproach(Mage.Get(), Building, Mage->AttackRange, MageGoal);
                const bool bSwordGoal = EnemyCombatNavigation::FindApproach(Original.Get(), Building, Original->AttackRange, SwordGoal);
                Building->GetActorBounds(true, BoundsCenter, BoundsExtent);
                Test->AddInfo(FString::Printf(TEXT("Approach diagnostic mage=%d sword=%d boundsCenter=%s boundsExtent=%s mageCapsule=%.1f/%.1f swordCapsule=%.1f/%.1f"),
                    bMageGoal, bSwordGoal, *BoundsCenter.ToString(), *BoundsExtent.ToString(),
                    Mage->GetCapsuleComponent()->GetScaledCapsuleRadius(), Mage->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
                    Original->GetCapsuleComponent()->GetScaledCapsuleRadius(), Original->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
                auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
                FNavLocation Goal;
                if (Nav && Nav->ProjectPointToNavigation(Building->GetActorLocation(), Goal, FVector(600.f, 600.f, 300.f)))
                {
                    auto* Path = Nav->FindPathToLocationSynchronously(World, Mage->GetActorLocation(), Goal.Location, Mage.Get());
                    Test->AddInfo(FString::Printf(TEXT("Approach path valid=%d partial=%d projectedCap=%s points=%d startOffset=%.1f"),
                        IsValid(Path) && Path->IsValid(), IsValid(Path) && Path->IsPartial(), *Goal.Location.ToString(),
                        IsValid(Path) ? Path->PathPoints.Num() : 0,
                        IsValid(Path) && !Path->PathPoints.IsEmpty() ? FVector::Dist2D(Mage->GetActorLocation(), Path->PathPoints[0]) : -1.f));
                }
            }
            if (Mage->bRemovingBarrier)
            {
                bSawBarrierRemoval = true;
                Test->TestFalse(TEXT("No cast overlaps barrier removal"), Mage->bCastingBarrier);
                if (auto* Anim = Mage->GetMesh()->GetAnimInstance())
                    if (auto* Montage = Mage->PlayingBarrierRemovalMontage.Get())
                    {
                        const float Position = Anim->Montage_GetPosition(Montage);
                        bSawReverseProgress |= PreviousRemovalPosition > Position + 0.001f;
                        PreviousRemovalPosition = Position;
                    }
            }
            else PreviousRemovalPosition = -1.f;
            if (Mage->HasActiveBarrier() && !Mage->bCastingBarrier)
                for (const auto& Weak : Mage->JoinedEscort)
                    if (Weak.IsValid() && Weak->IsAlive())
                        MaxGap = FMath::Max(MaxGap, FVector::Dist2D(Mage->GetActorLocation(), Weak->GetActorLocation()));
            if (Now - LastSample >= 0.2f)
            {
                const bool bCruising = Mage->IsEscortMarching() && !Mage->bCastingBarrier &&
                    !Mage->bApproachingMeeting && !Mage->bRecruitBarrierRefreshCasting &&
                    !Mage->bFinishingEscortMarch && !Original->IsWaitingForMage() && Now - Start > 4.f;
                if (bCruising && FVector::Dist2D(LastPosition, Original->GetActorLocation()) < 0.2f)
                    CurrentStop += Now - LastSample;
                else CurrentStop = 0.f;
                LongestStop = FMath::Max(LongestStop, CurrentStop);
                LastPosition = Original->GetActorLocation();
                LastSample = Now;
            }
        }
        if (Now - Start < 90.f && !bMageAttacked) return false;
        Test->TestTrue(TEXT("Level_1 mage spawned"), Mage.IsValid());
        Test->TestTrue(TEXT("Level_1 paired swordsman spawned"), Original.IsValid());
        if (Mage.IsValid() && Original.IsValid())
        {
            const float Travel = FVector::Dist2D(OriginalStart, Original->GetActorLocation());
            Test->AddInfo(FString::Printf(TEXT("Level_1 travel=%.1f maxDomeGap=%.1f longestCruiseStop=%.2f mageAttacked=%d mage=%s sword=%s"),
                Travel, MaxGap, LongestStop, bMageAttacked, *Mage->GetActorLocation().ToString(), *Original->GetActorLocation().ToString()));
            FVector Surface;
            auto* Building = Cast<ARICBuilding>(Mage->Capitol.Get());
            EnemyCombatNavigation::GetSurfacePoint(Original->GetActorLocation(), Building, Surface);
            Test->AddInfo(FString::Printf(TEXT("Final reach distance=%.3f range=%.1f canAttack=%d waiting=%d wall=%d capitol=%d march=%d casting=%d removing=%d recruitCast=%d surface=%s"),
                EnemyCombatNavigation::SurfaceDistance(Original->GetActorLocation(), Building), Original->AttackRange,
                EnemyCombatNavigation::CanAttack(Original.Get(), Building, Original->AttackRange), Original->IsWaitingForMage(),
                Original->IsAttackingWall(), Original->IsAttackingCapitol(), Mage->bEscortMarchActive,
                Mage->bCastingBarrier, Mage->bRemovingBarrier, Mage->bRecruitBarrierRefreshCasting, *Surface.ToString()));
            FCollisionObjectQueryParams Objects;
            Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(EscortTestReach), false, Original.Get());
            FHitResult Hit;
            World->LineTraceSingleByObjectType(Hit, Original->GetActorLocation(),
                Surface + (Surface - Original->GetActorLocation()).GetSafeNormal() * 2.f, Objects, Query);
            Test->AddInfo(FString::Printf(TEXT("Final melee ray hits %s; goal=%s"), *GetNameSafe(Hit.GetActor()),
                *Mage->MarchDestinations.FindRef(Original.Get()).ToString()));
            Test->TestFalse(TEXT("Level_1 preserves original companion"), bLostOriginal);
            Test->TestTrue(TEXT("Level_1 pair travels together"), Travel > 400.f);
            Test->TestTrue(TEXT("Level_1 all joined companions remain inside 180 cm dome"), MaxGap <= 180.f);
            Test->TestTrue(TEXT("Level_1 no repeated cruise freeze"), LongestStop < 1.f);
            Test->TestTrue(TEXT("Level_1 mage joins Capitol attack"), bMageAttacked);
            Test->TestTrue(TEXT("Real Blueprint plays barrier removal animation"), bSawBarrierRemoval);
            if (!Mage->BarrierRemovalMontage)
                Test->TestTrue(TEXT("Fallback barrier montage actually runs backwards"), bSawReverseProgress);
        }
        return true;
    }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AEnemyMageAI> Mage;
    TWeakObjectPtr<AEnemySwordsmanAI> Original;
    FVector OriginalStart, LastPosition;
    float Start = -1.f, LastSample = 0.f, CurrentStop = 0.f, LongestStop = 0.f, MaxGap = 0.f;
    bool bLostOriginal = false, bMageAttacked = false;
    bool bSawBarrierRemoval = false, bSawReverseProgress = false;
    bool bLoggedApproach = false;
    float PreviousRemovalPosition = -1.f;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyLevelOneTest, "RaidIsComing.AI.LevelOneEscort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyLevelOneTest::RunTest(const FString& Parameters)
{
    // Executed in a separate unattended editor. The map is loaded and played,
    // never saved; its spawner, balance, navmesh and montage defaults are real.
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
    if (!EditorWorld || EditorWorld->GetOutermost()->GetName() != TEXT("/Game/Levels/Level_1"))
        FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Levels/Level_1"));
    EditorWorld = GEditor->GetEditorWorldContext().World();
    // Headless fixed-time tests can reach PIE before the editor's asynchronous
    // initial navmesh build has finished. Duplicate completed navigation data.
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EditorWorld))
    {
        Nav->Build();
        for (ANavigationData* Data : Nav->NavDataSet) if (Data) Data->EnsureBuildCompletion();
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FObserveLevelOneCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
