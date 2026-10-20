#include "RICPassageLibrary.h"

#include "RICBuilding.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

TArray<ARICPassage*> URICPassageLibrary::GetAllPassages(
    const UObject* WorldContextObject)
{
    TArray<ARICPassage*> Result;
    if (!GEngine || !IsValid(WorldContextObject))
    {
        return Result;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject, EGetWorldErrorMode::ReturnNull);
    if (!World)
    {
        return Result;
    }

    for (TActorIterator<ARICPassage> It(World); It; ++It)
    {
        ARICPassage* Passage = *It;
        if (IsValid(Passage) && !Passage->IsActorBeingDestroyed())
        {
            Result.Add(Passage);
        }
    }
    return Result;
}

TArray<FRICPassageData> URICPassageLibrary::GetAllPassageData(
    const UObject* WorldContextObject)
{
    TArray<FRICPassageData> Result;
    const TArray<ARICPassage*> Passages = GetAllPassages(WorldContextObject);
    Result.Reserve(Passages.Num());
    for (ARICPassage* Passage : Passages)
    {
        if (IsValid(Passage) && !Passage->IsActorBeingDestroyed())
        {
            Result.Add(Passage->GetPassageData());
        }
    }
    return Result;
}

void URICPassageLibrary::LogPassageData(const UObject* WorldContextObject)
{
    const TArray<FRICPassageData> Entries = GetAllPassageData(WorldContextObject);
    UE_LOG(LogTemp, Log, TEXT("[PassageAI] Count=%d"), Entries.Num());

    for (const FRICPassageData& Data : Entries)
    {
        UE_LOG(LogTemp, Log,
            TEXT("[PassageAI] Passage=%s ID=%s Ring=%d Open=%s PassagePosition=%s Barrier=%s BarrierID=%s HP=%.1f/%.1f BarrierPosition=%s"),
            *GetNameSafe(Data.Passage.Get()),
            *Data.PassageId.ToString(),
            Data.RingId,
            Data.bIsOpen ? TEXT("Yes") : TEXT("No"),
            *Data.PassageLocation.ToString(),
            *GetNameSafe(Data.Barrier.Get()),
            *Data.BarrierId.ToString(),
            Data.BarrierHP,
            Data.BarrierMaxHP,
            *Data.BarrierLocation.ToString());
    }
}
