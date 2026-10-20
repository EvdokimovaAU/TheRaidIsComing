#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RICPassage.h"
#include "RICPassageLibrary.generated.h"

UCLASS()
class RAIDISCOMING_API URICPassageLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Поиск ограничен миром вызывающего объекта. Порядок результатов не гарантирован.
    UFUNCTION(BlueprintCallable, Category = "Construction|Passages",
        meta = (WorldContext = "WorldContextObject"))
    static TArray<ARICPassage*> GetAllPassages(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "Construction|Passages",
        meta = (WorldContext = "WorldContextObject"))
    static TArray<FRICPassageData> GetAllPassageData(const UObject* WorldContextObject);

    // Явный диагностический запрос; сам по себе не запускается по Tick.
    UFUNCTION(BlueprintCallable, Category = "Construction|Passages|Debug",
        meta = (WorldContext = "WorldContextObject"))
    static void LogPassageData(const UObject* WorldContextObject);
};
