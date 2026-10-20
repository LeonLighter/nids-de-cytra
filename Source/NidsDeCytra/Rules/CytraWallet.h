// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The team's gold, with the totals shown on the end screen. */
struct FCytraWallet
{
	int32 Gold = 0;
	int32 Spent = 0;
	int32 Mined = 0;
	int32 Refunded = 0;

	FCytraWallet() = default;
	explicit FCytraWallet(int32 StartGold) : Gold(StartGold) {}

	bool CanAfford(int32 Cost) const { return Gold >= Cost; }

	/** Gold still needed to pay Cost, 0 if affordable. */
	int32 Missing(int32 Cost) const { return FMath::Max(0, Cost - Gold); }

	/** Pays Cost and counts it as spent. Leaves the wallet unchanged and returns false if gold is short. */
	bool TrySpend(int32 Cost)
	{
		if (!CanAfford(Cost))
		{
			return false;
		}
		Gold -= Cost;
		Spent += Cost;
		return true;
	}

	/** Kills, wave bonuses and early launch bonuses. */
	void Earn(int32 Amount) { Gold += Amount; }
	void EarnFromMine(int32 Amount) { Gold += Amount; Mined += Amount; }
	void EarnRefund(int32 Amount) { Gold += Amount; Refunded += Amount; }
};
