#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AI/EnemyMageAI.h"
#include "AI/EnemySwordsmanAI.h"
#include "AI/EnemyBarrier.h"
#include "AI/EnemyCombatNavigation.h"
#include "Construction/RICBuilding.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/NavMeshBoundsVolume.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyEscortObstacleTest,
    "RaidIsComing.AI.EscortObstacle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyEscortObstacleTest::RunTest(const FString& Parameters)
{
    // Отдельный тестовый мир.
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    struct FCleanup
    {
        UWorld* World;
        ~FCleanup()
        {
            World->BeginTearingDown();
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    } Cleanup{World};
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Collision fixture mesh"), Cube)) return false;
    auto* Floor = World->SpawnActor<AStaticMeshActor>();
    Floor->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Floor->SetActorScale3D(FVector(20.f, 12.f, 0.2f));
    Floor->SetActorLocation(FVector(0.f, 0.f, -10.f));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
    auto* BoundsBox = NewObject<UBoxComponent>(Bounds);
    BoundsBox->SetBoxExtent(FVector(1000.f, 600.f, 400.f));
    BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BoundsBox->SetCanEverAffectNavigation(false);
    BoundsBox->RegisterComponent();
    BoundsBox->AttachToComponent(Bounds->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Bounds->AddInstanceComponent(BoundsBox);

    auto* Wall = World->SpawnActor<ARICBuilding>();
    Wall->BuildingType = ERICBuildingType::Barrier;
    Wall->BuildingMesh->SetStaticMesh(Cube);
    Wall->SetActorScale3D(FVector(1.f, 14.f, 4.f));
    Wall->SetActorLocation(FVector(0.f, 0.f, 200.f));
    Wall->SetCanBeDamaged(true);
    Wall->CurrentHP = Wall->MaxHP = 10000.f;

    FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode);
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!TestNotNull(TEXT("Navigation system"), Nav)) return false;
    Nav->OnNavigationBoundsUpdated(Bounds);
    Nav->Build();
    for (ANavigationData* Data : Nav->NavDataSet) if (Data) Data->EnsureBuildCompletion();

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mage = World->SpawnActor<AEnemyMageAI>(FVector(-450.f, 0.f, 100.f), FRotator::ZeroRotator, Spawn);
    auto* First = World->SpawnActor<AEnemySwordsmanAI>(FVector(-400.f, -100.f, 100.f), FRotator::ZeroRotator, Spawn);
    auto* Second = World->SpawnActor<AEnemySwordsmanAI>(FVector(-400.f, 100.f, 100.f), FRotator::ZeroRotator, Spawn);
    Mage->BarrierClass = AEnemyBarrier::StaticClass();
    Mage->SpawnDefaultController(); First->SpawnDefaultController(); Second->SpawnDefaultController();
    First->TryAssignEscortMage(Mage); Second->TryAssignEscortMage(Mage);
    Mage->EscortGroup = {First, Second};
    Mage->JoinedEscort.Add(First); Mage->JoinedEscort.Add(Second);
    Mage->FollowedSwordsman = First;
    Mage->Barrier();

    TestFalse(TEXT("Separate NavMesh islands do not count as a complete path"),
        EnemyCombatNavigation::HasCompletePath(Mage, FVector(500.f, 0.f, 0.f)));
    FVector Approach;
    if (!TestTrue(TEXT("Wall has an approach on the current island"),
        EnemyCombatNavigation::FindApproach(Mage, Wall, Mage->WallAttackDistance, Approach))) return false;
    TestTrue(TEXT("Approach stays on near side"), Approach.X < -50.f);
    TestFalse(TEXT("Collision distance rejects remote attacks"),
        EnemyCombatNavigation::CanAttack(Mage, Wall, Mage->WallAttackDistance));

    // A joined ally may be waiting because of the preceding cast. It must not
    // deadlock the group approach, which takes over the waiting state.
    First->WaitForMage(Mage);
    if (!TestTrue(TEXT("Group plan commits"), Mage->BeginObstacleAction(Wall))) return false;
    TestEqual(TEXT("One reserved attack position per member"), Mage->ObstacleSlots.Num(), 3);
    TestFalse(TEXT("Joined waiting state cleared on approach"), First->IsWaitingForMage());
    const float InitialHP = Wall->CurrentHP;
    First->AttackWall();
    TestEqual(TEXT("No damage before group authorization"), Wall->CurrentHP, InitialHP);

    // Set fixture transforms to the verified destinations to exercise readiness
    // transitions independently of animation and character acceleration.
    for (int32 Index = 0; Index < Mage->ObstacleSlots.Num() - 1; ++Index)
    {
        const auto& Slot = Mage->ObstacleSlots[Index];
        auto* Member = Slot.Member.Get();
        Member->SetActorLocation(Slot.Destination + FVector(0.f, 0.f,
            Member->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    }
    Mage->UpdateObstacleAction();
    TestTrue(TEXT("Barrier remains until last member arrives"), Mage->HasActiveBarrier());
    TestEqual(TEXT("No premature group damage"), Wall->CurrentHP, InitialHP);
    const auto& Last = Mage->ObstacleSlots.Last();
    Last.Member->SetActorLocation(Last.Destination + FVector(0.f, 0.f,
        Last.Member->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    Mage->UpdateObstacleAction();
    TestFalse(TEXT("Barrier removed for group attack"), Mage->HasActiveBarrier());
    TestTrue(TEXT("Group begins damage after readiness"), Wall->CurrentHP < InitialHP);
    TestTrue(TEXT("Second swordsman authorized"), Second->IsEscortWallAttackAuthorized());

    First->bIsDead = true;
    Mage->RefreshEscortGroup(); Mage->SelectSwordsman();
    TestTrue(TEXT("Survivor becomes leader"), Mage->FollowedSwordsman.Get() == Second);
    Wall->Destroy();
    Mage->UpdateObstacleAction();
    TestFalse(TEXT("Survivor stops wall attack on disappearance"), Second->IsAttackingWall());
    TestTrue(TEXT("Group held for restoration"), Mage->OwnsEscortMovement());
    Mage->UpdateObstacleAction();
    TestTrue(TEXT("Dome restored"), Mage->HasActiveBarrier());
    TestFalse(TEXT("Movement released after completed cast"), Mage->OwnsEscortMovement());
    Mage->ReleaseEscortGroup();
    TestFalse(TEXT("Mage release frees survivor"), Second->HasEscortMage());

    // Verify the real building Blueprints support the collision query used by
    // both attack positioning and damage, rather than silently relying on AABBs.
    const TCHAR* BuildingClasses[] = {
        TEXT("/Game/Blueprints/Buildings/BP_Barrier.BP_Barrier_C"),
        TEXT("/Game/Blueprints/Buildings/BP_Capitol.BP_Capitol_C")
    };
    for (const TCHAR* Path : BuildingClasses)
    {
        auto* Class = LoadClass<ARICBuilding>(nullptr, Path);
        if (!TestNotNull(FString::Printf(TEXT("Building class %s"), Path), Class)) continue;
        auto* Building = World->SpawnActor<ARICBuilding>(Class, FVector(2500.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
        const FBox Box = Building->GetComponentsBoundingBox(true);
        const FVector Probe(Box.Max.X + 100.f, Box.GetCenter().Y, Box.GetCenter().Z);
        TInlineComponentArray<UPrimitiveComponent*> Primitives(Building);
        for (auto* Component : Primitives)
        {
            FVector Point;
            AddInfo(FString::Printf(TEXT("%s component=%s enabled=%d pawn=%d closest=%.1f bounds=%s probe=%s"),
                Path, *Component->GetName(), static_cast<int32>(Component->GetCollisionEnabled()),
                static_cast<int32>(Component->GetCollisionResponseToChannel(ECC_Pawn)),
                Component->GetClosestPointOnCollision(Probe, Point),
                *Component->Bounds.ToString(), *Probe.ToString()));
        }
        TestTrue(FString::Printf(TEXT("Actual collision supports surface distance: %s"), Path),
            EnemyCombatNavigation::SurfaceDistance(Probe, Building) < 1000.f);
        Building->Destroy();
    }
    return true;
}
#endif
