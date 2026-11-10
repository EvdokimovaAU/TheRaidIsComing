#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "Construction/RICConstructionComponent.h"
#include "Construction/RICBuilding.h"
#include "Construction/RICBuildZone.h"
#include "Construction/RICMoneyComponent.h"
#include "Construction/RICPlacementMath.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRICBuildMenuPlacementTest,
    "RaidIsComing.Construction.BuildMenu.SavedPosition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRICBuildMenuPlacementTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("Controller"), Controller)) return false;
    Controller->SetAsLocalPlayerController();
    URICMoneyComponent* Money = NewObject<URICMoneyComponent>(Controller);
    Controller->AddInstanceComponent(Money);
    Money->RegisterComponent();
    Money->CurrentMoney = 1000;
    URICConstructionComponent* Construction = NewObject<URICConstructionComponent>(Controller);
    Controller->AddInstanceComponent(Construction);
    Construction->RegisterComponent();
    ARICBuildZone* Zone = World->SpawnActor<ARICBuildZone>();
    Zone->HalfSize = FVector2D(500.0, 500.0);

    UClass* TowerClass = LoadClass<ARICBuilding>(nullptr,
        TEXT("/Game/Blueprints/Buildings/BP_ArcherTower.BP_ArcherTower_C"));
    if (!TestNotNull(TEXT("Existing archer Blueprint"), TowerClass)) return false;
    ARICBuilding* Defaults = TowerClass->GetDefaultObject<ARICBuilding>();
    if (!TestNotNull(TEXT("Configured tower mesh"), Defaults->BuildingMesh->GetStaticMesh().Get())) return false;
    const int32 PreviousCost = Defaults->ConstructionCost;
    ON_SCOPE_EXIT
    {
        Construction->CancelPreview();
        Defaults->ConstructionCost = PreviousCost;
    };
    Defaults->ConstructionCost = 73;
    Zone->HalfSize = FVector2D(10000.0, 10000.0);

    FVector Ground(125.0, -80.0, 0.0);
    auto BeginFrozenSelection = [&]()
    {
        // Позиция для запуска теста без мыши.
        TGuardValue<TOptional<FVector>> PositionGuard(Construction->FixedGroundPosition, TOptional<FVector>(Ground));
        if (!Construction->StartPreview(TowerClass)) return false;
        FVector Saved;
        if (!Construction->FreezePreview(Saved)) return false;
        TestTrue(TEXT("Ground anchor preserved"), Saved.Equals(Ground, 0.01));
        TestTrue(TEXT("Preview hidden"), Construction->PreviewActor->IsHidden());
        TestFalse(TEXT("Frozen preview does not tick"), Construction->IsComponentTickEnabled());
        return true;
    };
    if (!TestTrue(TEXT("Open selection"), BeginFrozenSelection())) return false;
    Construction->TickComponent(0.016f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Tick cannot reveal frozen preview"), Construction->PreviewActor->IsHidden());
    TestTrue(TEXT("Place via existing pipeline"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Exactly one payment"), Money->CurrentMoney, 927);
    int32 Count = 0;
    for (TActorIterator<ARICBuilding> It(World); It; ++It)
    {
        ++Count;
        const FVector Center = It->BuildingMesh->Bounds.Origin;
        TestTrue(TEXT("Footprint at saved XY, not cursor"), FVector2D(Center).Equals(FVector2D(Ground), 0.01));
    }
    TestEqual(TEXT("Exactly one building"), Count, 1);
    TestFalse(TEXT("Duplicate selection rejected"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Duplicate does not charge"), Money->CurrentMoney, 927);

    TestTrue(TEXT("Select occupied position"), BeginFrozenSelection());
    TestFalse(TEXT("Existing overlap check rejects"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Overlap does not charge"), Money->CurrentMoney, 927);
    for (TActorIterator<ARICBuilding> It(World); It; ++It) It->Destroy();

    Money->CurrentMoney = 0;
    TestTrue(TEXT("Select without money"), BeginFrozenSelection());
    TestFalse(TEXT("Existing economy rejects"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Balance unchanged"), Money->CurrentMoney, 0);

    Money->CurrentMoney = 1000;
    TestTrue(TEXT("Select before zone changes"), BeginFrozenSelection());
    Zone->bAllowArcherTowers = false;
    TestFalse(TEXT("Zone revalidated after opening menu"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Invalid zone does not charge"), Money->CurrentMoney, 1000);
    Zone->bAllowArcherTowers = true;
    TestTrue(TEXT("Select before available area shrinks"), BeginFrozenSelection());
    Zone->HalfSize = FVector2D(130.0, 130.0);
    TestFalse(TEXT("Saved anchor cannot snap to fit a smaller area"), Construction->TryPlaceAt(TowerClass, Ground));
    TestEqual(TEXT("Out-of-bounds does not charge"), Money->CurrentMoney, 1000);
    Zone->HalfSize = FVector2D(10000.0, 10000.0);
    TestTrue(TEXT("Select then cancel"), BeginFrozenSelection());
    Construction->CancelPreview();
    TestFalse(TEXT("Cancel deletes preview"), Construction->IsBuildingMode());
    TestEqual(TEXT("Cancel does not charge"), Money->CurrentMoney, 1000);

    UClass* MageClass = LoadClass<ARICBuilding>(nullptr,
        TEXT("/Game/Blueprints/Buildings/BP_MageTower.BP_MageTower_C"));
    if (!TestNotNull(TEXT("Existing mage Blueprint"), MageClass)) return false;
    const int32 MageCost = MageClass->GetDefaultObject<ARICBuilding>()->ConstructionCost;
    Money->CurrentMoney = FMath::Max(1000, MageCost);
    const int32 BeforeMage = Money->CurrentMoney;
    TestTrue(TEXT("Freeze archer before selecting mage"), BeginFrozenSelection());
    TestTrue(TEXT("Build mage from frozen archer selection"), Construction->TryPlaceAt(MageClass, Ground));
    TestEqual(TEXT("Mage uses its actual price"), Money->CurrentMoney, BeforeMage - MageCost);
    int32 MageCount = 0;
    for (TActorIterator<ARICBuilding> It(World); It; ++It)
    {
        if (!It->IsActorBeingDestroyed() && It->IsA(MageClass))
        {
            ++MageCount;
            TestTrue(TEXT("Mage footprint keeps saved XY"), FVector2D(It->BuildingMesh->Bounds.Origin).Equals(FVector2D(Ground), 0.01));
        }
    }
    TestEqual(TEXT("Exactly one mage"), MageCount, 1);

    for (TActorIterator<ARICBuilding> It(World); It; ++It) It->Destroy();
    // Смена меша с другим pivot у границы кольцевой зоны.
    Zone->ZoneShape = ERICBuildZoneShape::RingSector;
    Zone->InnerRadius = 440.0f;
    Zone->OuterRadius = 600.0f;
    Zone->StartAngleDegrees = 0.0f;
    Zone->ArcAngleDegrees = 90.0f;
    Zone->SetActorLocation(FVector(0.0, 0.0, 15.0));
    RICPlacement::FRegion Region;
    Region.bRing = true;
    Region.BuildingHalf = Construction->FootprintHalfSize;
    Region.Inner = Zone->InnerRadius;
    Region.Outer = Zone->OuterRadius;
    Region.StartDegrees = Zone->StartAngleDegrees;
    Region.ArcDegrees = Zone->ArcAngleDegrees;
    FVector2D RingCenter;
    if (!TestTrue(TEXT("Find existing ring boundary position"),
        RICPlacement::FindNearest(Region, FVector2D(900.0, 900.0), RingCenter))) return false;
    Ground = FVector(RingCenter.X, RingCenter.Y, 15.0);
    Money->CurrentMoney = FMath::Max(1000, MageCost);
    const int32 BeforeRingMage = Money->CurrentMoney;
    TestTrue(TEXT("Freeze archer at ring boundary"), BeginFrozenSelection());
    const FVector ArcherCenter = Construction->PreviewActor->GetStaticMeshComponent()->Bounds.Origin;
    TestTrue(TEXT("Saved point is actual footprint center"), FVector2D(ArcherCenter).Equals(FVector2D(Ground), 0.01));
    TestTrue(TEXT("Mage fits same footprint despite different pivot"), Construction->TryPlaceAt(MageClass, Ground));
    TestEqual(TEXT("Ring mage charged once"), Money->CurrentMoney, BeforeRingMage - MageCost);
    return true;
}
#endif
