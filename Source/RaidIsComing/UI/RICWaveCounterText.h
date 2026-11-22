#pragma once

#include "Components/TextBlock.h"
#include "RICWaveCounterText.generated.h"

class AWaveManager;

UCLASS(meta = (DisplayName = "Wave Counter"))
class URICWaveCounterText : public UTextBlock
{
    GENERATED_BODY()

protected:
    virtual void SynchronizeProperties() override;

private:
    FText GetWaveCounterText() const;
    TWeakObjectPtr<AWaveManager> WaveManager;
};
