#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "Construction/RICBuilding.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEscortMarchTest, "RaidIsComing.AI.EscortMarch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyEscortMarchTest::RunTest(const FString& Parameters)
{
    for (int32 Scenario = 0; Scenario < 3; ++Scenario)
    {
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("March test world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    struct FCleanup
    {
        UWorld* World;
        ~FCleanup()
        {
            // EndPlay releases physics callbacks (including the Water plugin)
            // before the isolated world's physics scene is destroyed.
            World->BeginTearingDown();
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    } Cleanup{World};
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Floor = World->SpawnActor<AStaticMeshActor>();
    Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Floor->SetActorScale3D(FVector(24.f, 24.f, 0.2f));
    Floor->SetActorLocation(FVector(0.f, 0.f, -10.f));
    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
    auto* Box = NewObject<UBoxComponent>(Bounds);
    Box->SetBoxExtent(FVector(1200.f, 1200.f, 500.f));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Box->SetCanEverAffectNavigation(false);
    Box->RegisterComponent();
    Box->AttachToComponent(Bounds->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Bounds->AddInstanceComponent(Box);
    auto* Capitol = World->SpawnActor<ARICBuilding>();
    Capitol->BuildingType = ERICBuildingType::Capitol;
    Capitol->BuildingMesh->SetStaticMesh(Cube);
    Capitol->SetActorScale3D(FVector(2.f, 3.f, 4.f));
    Capitol->SetActorLocation(FVector(0.f, 0.f, 200.f));
    Capitol->MaxHP = 10000.f;
    FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!TestNotNull(TEXT("March navigation"), Nav)) return false;
    Nav->OnNavigationBoundsUpdated(Bounds);
    Nav->Build();
    for (ANavigationData* Data : Nav->NavDataSet) if (Data) Data->EnsureBuildCompletion();

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mage = World->SpawnActor<AEnemyMageAI>(FVector(979.f, 57.f, 110.f), FRotator::ZeroRotator, Spawn);
    auto* Sword = World->SpawnActor<AEnemySwordsmanAI>(FVector(930.f, -10.f, 110.f), FRotator::ZeroRotator, Spawn);
    auto* FreeSword = World->SpawnActor<AEnemySwordsmanAI>(FVector(-460.f, 790.f, 110.f), FRotator::ZeroRotator, Spawn);
    Sword->MovementSpeed = FreeSword->MovementSpeed = 75.f; // Reported Level_1 balance.
    Mage->NormalMovementSpeed = 250.f;
    Mage->BarrierClass = AEnemyBarrier::StaticClass();
    Mage->AllyDetectionRadius = Scenario == 0 ? 260.f : 800.f;
    AEnemySwordsmanAI* Recruit = Scenario == 0 ? nullptr : World->SpawnActor<AEnemySwordsmanAI>(
        FVector(450.f, 80.f, 110.f), FRotator::ZeroRotator, Spawn);
    if (Recruit)
    {
        Recruit->MovementSpeed = 75.f;
        Recruit->SpawnDefaultController();
        Recruit->SetCapitol(Capitol);
    }
    Mage->SpawnDefaultController(); Sword->SpawnDefaultController(); FreeSword->SpawnDefaultController();
    Mage->SetCapitol(Capitol); Sword->SetCapitol(Capitol); FreeSword->SetCapitol(Capitol);
    Mage->BindSpawnCompanion(Sword);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    World->BeginPlay();

    const FVector Start = Sword->GetActorLocation();
    const FVector FreeStart = FreeSword->GetActorLocation();
    bool bJoined = false, bReleased = false, bSawBarrier = false, bMageAttacked = false;
    bool bEarlyRelease = false, bOverspeed = false;
    float LongestStop = 0.f, CurrentStop = 0.f;
    bool bRecruitReserved = false, bRecruitJoined = false, bRecruitCancelled = false;
    int32 DomeCount = 0;
    TWeakObjectPtr<AEnemyBarrier> LastDome;
    for (int32 Frame = 0; Frame < (Scenario == 2 ? 1800 : 900); ++Frame)
    {
        // TimerManager ticks once per engine frame. This isolated synchronous
        // simulation advances that frame as well as the world's delta time.
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 1.f / 30.f);
        if (Recruit)
        {
            bRecruitReserved |= Mage->MeetingRecruit.Get() == Recruit;
            bRecruitJoined |= Mage->JoinedEscort.Contains(Recruit);
            bRecruitCancelled |= bRecruitReserved && !Recruit->HasEscortMage() && !Recruit->IsWaitingForMage();
        }
        if (Scenario == 2 && Frame == 45)
        {
            auto* Detour = World->SpawnActor<AStaticMeshActor>();
            Detour->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            Detour->GetStaticMeshComponent()->SetStaticMesh(Cube);
            Detour->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            Detour->SetActorScale3D(FVector(1.f, 12.f, 4.f));
            Detour->SetActorLocation(FVector(650.f, 0.f, 200.f));
            Detour->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
            Nav->Build();
            for (ANavigationData* Data : Nav->NavDataSet) if (Data) Data->EnsureBuildCompletion();
            Mage->OnCombatNavigationChanged(nullptr);
            Mage->ClearMarchPlan();
        }
        bSawBarrier |= Mage->HasActiveBarrier();
        if (Mage->HasActiveBarrier() && LastDome.Get() != Mage->ActiveBarrier.Get())
        {
            ++DomeCount;
            LastDome = Mage->ActiveBarrier.Get();
        }
        if (Mage->HasActiveBarrier()) Mage->ActiveBarrier->BarrierRadius = 180.f;
        bJoined |= Mage->JoinedEscort.Contains(Sword);
        bReleased |= bJoined && !Sword->IsAssignedToMage(Mage);
        bMageAttacked |= Mage->bAttacking;
        if (bJoined && !bSawBarrier && !Mage->bCastingBarrier && !Sword->IsWaitingForMage()) bEarlyRelease = true;
        if (bJoined && bSawBarrier && Mage->IsEscortMarching() && !Mage->bCastingBarrier &&
            !Mage->bApproachingMeeting && !Mage->bRecruitBarrierRefreshCasting &&
            !Mage->bAttacking && !Sword->IsAttackingCapitol())
        {
            bOverspeed |= Mage->GetCharacterMovement()->MaxWalkSpeed > 75.1f;
            if (Frame > 60 && !Sword->IsWaitingForMage() &&
                FVector::Dist2D(Sword->GetActorLocation(), Capitol->GetActorLocation()) > 350.f &&
                Sword->GetVelocity().Size2D() < 1.f)
                CurrentStop += 1.f / 30.f;
            else CurrentStop = 0.f;
            LongestStop = FMath::Max(LongestStop, CurrentStop);
        }
    }
    AddInfo(FString::Printf(TEXT("Mage=%s Sword=%s free=%s longestCruiseStop=%.2f mageAttacked=%d"),
        *Mage->GetActorLocation().ToString(), *Sword->GetActorLocation().ToString(),
        *FreeSword->GetActorLocation().ToString(), LongestStop, bMageAttacked));
    TestTrue(TEXT("Original spawn companion joins"), bJoined);
    TestFalse(TEXT("Joined spawn companion remains assigned"), bReleased);
    TestFalse(TEXT("No release before first dome"), bEarlyRelease);
    TestFalse(TEXT("Mage does not outrun 75 cm/s swordsman while marching"), bOverspeed);
    TestTrue(TEXT("Companion actually moves with character/path-following simulation"),
        FVector::Dist2D(Start, Sword->GetActorLocation()) > 400.f);
    TestTrue(TEXT("Free swordsman moves independently"), FVector::Dist2D(FreeStart, FreeSword->GetActorLocation()) > 400.f);
    TestTrue(TEXT("No sustained cruise stop away from Capitol"), LongestStop < 0.7f);
    TestTrue(TEXT("Mage joins Capitol attack"), bMageAttacked);
    if (Scenario == 1)
    {
        TestTrue(TEXT("Recruit reserves an actual meeting point"), bRecruitReserved);
        TestTrue(TEXT("Recruit joins after meeting"), bRecruitJoined);
        TestEqual(TEXT("Initial dome and exactly one meeting replacement"), DomeCount, 2);
        TestEqual(TEXT("Group contains at most two swordsmen"), Mage->JoinedEscort.Num(), 2);
    }
    if (Scenario == 2)
    {
        TestTrue(TEXT("Recruit initially reserved before detour"), bRecruitReserved);
        TestTrue(TEXT("Detour cancels obsolete rendezvous and releases recruit"), bRecruitCancelled);
        TestFalse(TEXT("Cancelled recruit does not remain waiting"), Recruit->IsWaitingForMage());
    }
    AddInfo(FString::Printf(TEXT("Scenario=%d reserved=%d joined=%d cancelled=%d domes=%d"),
        Scenario, bRecruitReserved, bRecruitJoined, bRecruitCancelled, DomeCount));
    }
    return true;
}
#endif
