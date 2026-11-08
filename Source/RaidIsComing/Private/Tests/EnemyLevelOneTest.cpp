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
#include "EngineUtils.h"
#include "Editor.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

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
            for (TActorIterator<AWaveManager> It(World); It; ++It) { It->StartNextWave(); break; }
        }
        if (!Mage.IsValid())
        {
            for (TActorIterator<AEnemyMageAI> It(World); It; ++It)
            {
                Mage = *It;
                Original = It->FollowedSwordsman;
                Test->TestTrue(TEXT("Level_1 spawn pair bound before first behavior tick"), Original.IsValid());
                if (Original.IsValid()) { OriginalStart = Original->GetActorLocation(); LastPosition = OriginalStart; }
                break;
            }
        }
        const float Now = World->GetTimeSeconds();
        if (Mage.IsValid() && Original.IsValid())
        {
            bLostOriginal |= !Original->IsAssignedToMage(Mage.Get());
            bMageAttacked |= Mage->bAttacking;
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
            Test->TestFalse(TEXT("Level_1 preserves original companion"), bLostOriginal);
            Test->TestTrue(TEXT("Level_1 pair travels together"), Travel > 400.f);
            Test->TestTrue(TEXT("Level_1 all joined companions remain inside 180 cm dome"), MaxGap <= 180.f);
            Test->TestTrue(TEXT("Level_1 no repeated cruise freeze"), LongestStop < 1.f);
            Test->TestTrue(TEXT("Level_1 mage joins Capitol attack"), bMageAttacked);
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
