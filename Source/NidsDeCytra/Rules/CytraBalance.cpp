// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraBalance.h"

#include "Rules/CytraMath.h"

FCytraTowerStats FCytraBalance::TowerStats(FName Tower, int32 Level, bool bHighSlot) const
{
	const FCytraTowerDef& Def = Towers.FindChecked(Tower);
	FCytraTowerStats Stats;
	Stats.Range = Def.Range * (bHighSlot ? HighSlotRangeBonus : 1.0) * (1.0 + Upgrade.RangePerLevel * (Level - 1));
	Stats.Damage = Def.Damage * (1.0 + Upgrade.DamagePerLevel * (Level - 1));
	Stats.Rate = Def.Rate;
	return Stats;
}

int32 FCytraBalance::UpgradeCost(FName Tower, int32 CurrentLevel) const
{
	const FCytraTowerDef& Def = Towers.FindChecked(Tower);
	return CytraMath::Round(Def.Cost * Upgrade.CostFactor * CurrentLevel / Upgrade.CostRounding) * Upgrade.CostRounding;
}

int32 FCytraBalance::DismantleRefundOf(int32 Invested) const
{
	return CytraMath::Round(Invested * DismantleRefund);
}

int32 FCytraBalance::EnemyHp(FName Enemy, int32 Wave) const
{
	return CytraMath::Round(Enemies.FindChecked(Enemy).Hp * (1.0 + HpScalePerWave * (Wave - 1)));
}

int32 FCytraBalance::WaveEndBonus(int32 Wave) const
{
	return EndBonusBase + EndBonusPerWave * Wave;
}

int32 FCytraBalance::EarlyLaunchBonus(double Countdown) const
{
	return static_cast<int32>(FMath::FloorToDouble(FMath::Max(Countdown, 0.0))) * EarlyLaunchGoldPerSecond;
}
