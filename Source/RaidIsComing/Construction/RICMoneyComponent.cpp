
#include "RICMoneyComponent.h"
#include "AI/GameBalanceManager.h"

URICMoneyComponent::URICMoneyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URICMoneyComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AGameBalanceManager* Balance =
        AGameBalanceManager::GetBalanceManager(this))
    {
        InitialMoney =
            FMath::Max(0, Balance->General.StartingGold);
    }

    CurrentMoney = FMath::Max(0, InitialMoney);

    UE_LOG(LogTemp, Log,
        TEXT("[Money] Initialized: %d"),
        CurrentMoney);
}

bool URICMoneyComponent::CanAfford(int32 Cost) const
{
    return Cost >= 0 && CurrentMoney >= Cost;
}

bool URICMoneyComponent::TrySpend(int32 Cost)
{
    if (!CanAfford(Cost))
    {
        return false;
    }

    CurrentMoney -= Cost;

    UE_LOG(LogTemp, Log,
        TEXT("[Money] Spent: %d | Balance: %d"),
        Cost,
        CurrentMoney);

    return true;
}

int32 URICMoneyComponent::AddMoney(int32 Amount)
{
    if (Amount <= 0)
    {
        return 0;
    }

    const int64 NewBalance =
        static_cast<int64>(CurrentMoney) +
        static_cast<int64>(Amount);

    const int32 ClampedBalance = static_cast<int32>(
        FMath::Min<int64>(NewBalance, MAX_int32)
        );

    const int32 AddedAmount =
        ClampedBalance - CurrentMoney;

    CurrentMoney = ClampedBalance;

    UE_LOG(LogTemp, Log,
        TEXT("[Money] Added: %d | Balance: %d"),
        AddedAmount,
        CurrentMoney);

    return AddedAmount;
}
