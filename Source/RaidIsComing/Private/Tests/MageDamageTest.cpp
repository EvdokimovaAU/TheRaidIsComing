#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "AI/EnemyBarrier.h"
#include "AI/EnemyMageAI.h"
#include "AI/GameBalanceManager.h"
#include "AI/MageProjectile.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRICMageDamageTest,
    "RaidIsComing.AI.MageDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRICMageDamageTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Test world"), World)) return false;

    auto* Balance = World->SpawnActor<AGameBalanceManager>();
    auto* Mage = World->SpawnActor<AEnemyMageAI>();
    auto* Barrier = World->SpawnActor<AEnemyBarrier>();
    if (!TestNotNull(TEXT("Balance"), Balance) ||
        !TestNotNull(TEXT("Mage"), Mage) ||
        !TestNotNull(TEXT("Barrier"), Barrier)) return false;

    Balance->Mage.MaxHealth = 160.f;
    Balance->Mage.BarrierHealth = 24.f;
    Balance->General.BarrierDamageMultiplier = 0.5f;
    Mage->DispatchBeginPlay();
    TestEqual(TEXT("Mage starts with level HP"), Mage->CurrentHP, 160.f);
    TestEqual(TEXT("Mage receives level barrier HP"), Mage->BarrierHP, 24.f);
    Barrier->AttachToMage(Mage);
    Barrier->ActivateBarrier(Mage->BarrierHP, 0.f);

    auto FireAtMage = [&]()
    {
        const FVector SpawnLocation = Mage->GetActorLocation() + FVector(20.f, 0.f, 0.f);
        auto* Projectile = World->SpawnActor<AMageProjectile>(SpawnLocation, FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("Projectile"), Projectile)) return;
        Projectile->HitRadius = 40.f;
        Projectile->InitializeProjectile(Mage, 30.f, 0.f);
        Projectile->Tick(0.f);
    };

    FireAtMage();
    TestEqual(TEXT("First shot damages barrier with level multiplier"), Barrier->CurrentBarrierHP, 9.f);
    TestEqual(TEXT("Barrier protects mage"), Mage->CurrentHP, 160.f);
    FireAtMage();
    TestEqual(TEXT("Second shot drains barrier HP"), Barrier->CurrentBarrierHP, 0.f);
    TestFalse(TEXT("Barrier is inactive at zero HP"), Barrier->IsBarrierActive());
    TestEqual(TEXT("Breaking shot is absorbed"), Mage->CurrentHP, 160.f);
    FireAtMage();
    TestEqual(TEXT("Next shot damages mage with zero splash radius"), Mage->CurrentHP, 130.f);
    return true;
}

#endif
