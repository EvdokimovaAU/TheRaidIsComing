#include "UI/RICWaveCounterText.h"

#include "AI/WaveManager.h"
#include "EngineUtils.h"
#include "Widgets/Text/STextBlock.h"

void URICWaveCounterText::SynchronizeProperties()
{
    Super::SynchronizeProperties();

    if (!MyTextBlock.IsValid() || IsDesignTime() || !GetWorld())
    {
        return;
    }

    if (TActorIterator<AWaveManager> Manager(GetWorld()); Manager)
    {
        WaveManager = *Manager;
    }

    MyTextBlock->SetText(TAttribute<FText>::Create(
        TAttribute<FText>::FGetter::CreateUObject(this, &ThisClass::GetWaveCounterText)));
}

FText URICWaveCounterText::GetWaveCounterText() const
{
    const AWaveManager* Manager = WaveManager.Get();
    return Manager ? FText::Format(NSLOCTEXT("Waves", "Counter", "Волна {0} / {1}"),
        Manager->GetCurrentWaveNumber(), Manager->GetTotalWaves()) : FText::GetEmpty();
}
