#if WITH_EDITOR
#include "AI/WaveManager.h"
#include "AI/EnemySpawner.h"
#include "Construction/RICBuilding.h"
#include "Construction/RICConstructionComponent.h"
#include "Construction/RICMoneyComponent.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

struct FLevelTwoBalanceProbe
{
    static void Run(const TArray<FString>& Args, UWorld* World)
    {
        if (!World || !World->IsGameWorld() || !World->GetName().EndsWith(TEXT("Level_2"))) return;
        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC) return;
        auto* Construction = PC->FindComponentByClass<URICConstructionComponent>();
        auto* Money = PC->FindComponentByClass<URICMoneyComponent>();
        if (!Money || !Construction || Args.Num() < 3) return;
        const FVector Where(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), 30.f);
        if (Args[0] == TEXT("wave"))
        {
            for (TActorIterator<AWaveManager> Manager(World); Manager; ++Manager)
            {
                if (Manager->Waves.Num()!=1 || !Manager->Waves[0].SpawnGroups.IsEmpty()) return;
                FWaveSpawnGroup Group;
                Group.SpawnPoint = World->SpawnActor<ATargetPoint>(Where, FRotator::ZeroRotator);
                Group.SwordsmanCount = 1;
                Group.MageCount = 1;
                Manager->Waves[0].SpawnGroups.Add(Group);
                Manager->StartNextWave();
            }
            return;
        }
        UClass* TowerClass = LoadClass<ARICBuilding>(nullptr, Args[0]==TEXT("mage") ?
            TEXT("/Game/Blueprints/Buildings/BP_MageTower.BP_MageTower_C") :
            TEXT("/Game/Blueprints/Buildings/BP_ArcherTower.BP_ArcherTower_C"));
        TGuardValue<TOptional<FVector>> Guard(Construction->FixedGroundPosition,TOptional<FVector>(Where));
        bool Placed = Construction->StartPreview(TowerClass) && Construction->TryPlacePreview();
        Construction->CancelPreview();
        UE_LOG(LogTemp,Display,TEXT("BALANCE_TEST tower %s at %s placed=%d gold=%d"), *Args[0], *Where.ToString(),Placed,Money->CurrentMoney);
    }
};
static FAutoConsoleCommandWithWorldAndArgs TestLevelTwoBalance(TEXT("ric.TestLevelTwoBalance"),TEXT("Transient Level_2 verification"),FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FLevelTwoBalanceProbe::Run));
#endif
