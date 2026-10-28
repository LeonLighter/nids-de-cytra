// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraTileType.h"

/** Worker specialty. */
struct FCytraRoleDef
{
	FName Id;
	int32 RecruitCost = 0;
};

/** A job that opens a tile: clearing, bridge, ladder, opening the mine, raising the outpost. */
struct FCytraWorkDef
{
	FName Id;
	ECytraTileType On = ECytraTileType::Rock;
	FName Role;
	int32 Cost = 0;
	double Time = 0.0;
};

/** How a tower attacks. Each behavior is coded; towers sharing one only differ by data. */
enum class ECytraTowerBehavior : uint8
{
	/** Homing bolt on one target. */
	Bolt,
	/** Shell aimed ahead of the target, splash damage on impact. */
	Shell,
	/** Instant hit on the most advanced targets, which are slowed. */
	Chain,
};

struct FCytraTowerDef
{
	FName Id;
	ECytraTowerBehavior Behavior = ECytraTowerBehavior::Bolt;
	/** Builds and dismantles the tower. */
	FName Role;
	int32 Cost = 0;
	double Time = 0.0;
	double Range = 0.0;
	double Damage = 0.0;
	/** Shots per second. */
	double Rate = 0.0;
	double BoltSpeed = 0.0;
	double Splash = 0.0;
	double FlightTime = 0.0;
	int32 Targets = 1;
	/** Speed multiplier while slowed. */
	double Slow = 1.0;
	double SlowTime = 0.0;
};

struct FCytraEnemyDef
{
	FName Id;
	int32 Hp = 0;
	/** Cells per second. */
	double Speed = 0.0;
	int32 Gold = 0;
	int32 Eggs = 0;
};

struct FCytraUpgradeDef
{
	FName Role;
	double Time = 0.0;
	int32 MaxLevel = 1;
	double DamagePerLevel = 0.0;
	double RangePerLevel = 0.0;
	double CostFactor = 0.0;
	int32 CostRounding = 1;
};

struct FCytraTowerStats
{
	double Range = 0.0;
	double Damage = 0.0;
	double Rate = 0.0;
};

/** Every tunable value of the game, read from Content/Data/Balance.json. Distances are in cells, times in seconds. */
struct NIDSDECYTRA_API FCytraBalance
{
	int32 StartGold = 0;
	int32 StartEggs = 0;

	int32 MaxCrew = 0;
	double WorkerSpeed = 0.0;
	/** Workers present at the start, in creation order. */
	TArray<FName> StartRoles;
	/** Share of its specialty's recruitment cost paid back when a worker is dismissed. */
	double DismissRefund = 0.0;

	TMap<FName, FCytraRoleDef> Roles;
	TMap<FName, FCytraWorkDef> Works;
	TMap<FName, FCytraTowerDef> Towers;
	TMap<FName, FCytraEnemyDef> Enemies;

	FCytraUpgradeDef Upgrade;
	double DismantleTime = 0.0;
	/** Share of the invested gold given back by a dismantling. */
	double DismantleRefund = 0.0;
	double HighSlotRangeBonus = 1.0;

	int32 MineSlots = 0;
	double MinePeriod = 0.0;
	int32 MineYield = 0;

	double HpScalePerWave = 0.0;
	int32 EndBonusBase = 0;
	int32 EndBonusPerWave = 0;
	double PrepBase = 0.0;
	double PrepScale = 0.0;
	int32 ThreatLevels = 1;
	int32 EarlyLaunchGoldPerSecond = 0;

	/** Enemies left of this x (still off the map) cannot be targeted. */
	double TargetingMinX = 0.0;

	/** Stats of a tower at a level (1-based), on a high slot or not. The type must exist. */
	FCytraTowerStats TowerStats(FName Tower, int32 Level, bool bHighSlot) const;

	/** Price to raise a tower from CurrentLevel to the next. */
	int32 UpgradeCost(FName Tower, int32 CurrentLevel) const;

	/** Gold given back when dismantling a tower that cost Invested in total. */
	int32 DismantleRefundOf(int32 Invested) const;

	/** Hit points of an enemy spawned during wave Wave (1-based). */
	int32 EnemyHp(FName Enemy, int32 Wave) const;

	/** Gold earned when wave Wave (1-based) is cleared. */
	int32 WaveEndBonus(int32 Wave) const;

	/** Gold earned for launching the next wave with Countdown seconds left. */
	int32 EarlyLaunchBonus(double Countdown) const;
};
