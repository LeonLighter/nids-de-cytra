// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraBalanceLoader.h"

#include "Rules/CytraBalance.h"
#include "Rules/CytraJson.h"

using CytraJson::FReader;

namespace
{
	/** Visits each entry of an object of objects, in file order. Stops at the first failure. */
	template <typename FVisitor>
	bool ForEachEntry(const FReader& Parent, const TCHAR* Field, FString& Error, FVisitor&& Visit)
	{
		TSharedPtr<FJsonObject> Section;
		if (!Parent.Object(Field, Section))
		{
			return false;
		}
		if (Section->Values.IsEmpty())
		{
			return Parent.Fail(Field, TEXT("must not be empty"));
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Section->Values)
		{
			const FString Path = Parent.FieldPath(Field) + TEXT(".") + Entry.Key;
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Entry.Value->TryGetObject(Object))
			{
				Error = Path + TEXT(": must be an object");
				return false;
			}
			if (!Visit(FName(*Entry.Key), FReader(**Object, Path, Error)))
			{
				return false;
			}
		}
		return true;
	}

	bool CheckAtLeast(const FReader& Reader, const TCHAR* Field, double Value, double Min)
	{
		return Value >= Min || Reader.Fail(Field, FString::Printf(TEXT("must be at least %g"), Min));
	}

	bool CheckPositive(const FReader& Reader, const TCHAR* Field, double Value)
	{
		return Value > 0.0 || Reader.Fail(Field, TEXT("must be positive"));
	}

	bool CheckRole(const FCytraBalance& Balance, const FReader& Reader, const TCHAR* Field, FName Role)
	{
		return Balance.Roles.Contains(Role) || Reader.Fail(Field, FString::Printf(TEXT("unknown role '%s'"), *Role.ToString()));
	}

	bool ParseStart(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		TSharedPtr<FJsonObject> Object;
		if (!Root.Object(TEXT("start"), Object))
		{
			return false;
		}
		const FReader Start(*Object, TEXT("start"), Error);
		return Start.Integer(TEXT("gold"), Balance.StartGold) && CheckAtLeast(Start, TEXT("gold"), Balance.StartGold, 0)
			&& Start.Integer(TEXT("eggs"), Balance.StartEggs) && CheckAtLeast(Start, TEXT("eggs"), Balance.StartEggs, 1);
	}

	bool ParseRoles(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		return ForEachEntry(Root, TEXT("roles"), Error, [&Balance](FName Id, const FReader& Reader)
		{
			FCytraRoleDef Def;
			Def.Id = Id;
			if (!Reader.Integer(TEXT("recruitCost"), Def.RecruitCost) || !CheckAtLeast(Reader, TEXT("recruitCost"), Def.RecruitCost, 0))
			{
				return false;
			}
			Balance.Roles.Add(Id, Def);
			return true;
		});
	}

	bool ParseCrew(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		TSharedPtr<FJsonObject> Object;
		const TArray<TSharedPtr<FJsonValue>>* StartRoles = nullptr;
		if (!Root.Object(TEXT("crew"), Object))
		{
			return false;
		}
		const FReader Crew(*Object, TEXT("crew"), Error);
		if (!Crew.Integer(TEXT("max"), Balance.MaxCrew) || !CheckAtLeast(Crew, TEXT("max"), Balance.MaxCrew, 1)
			|| !Crew.Number(TEXT("speed"), Balance.WorkerSpeed) || !CheckPositive(Crew, TEXT("speed"), Balance.WorkerSpeed)
			|| !Crew.Array(TEXT("startRoles"), StartRoles))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *StartRoles)
		{
			const FName Role(*Value->AsString());
			if (!CheckRole(Balance, Crew, TEXT("startRoles"), Role))
			{
				return false;
			}
			Balance.StartRoles.Add(Role);
		}
		return Balance.StartRoles.Num() <= Balance.MaxCrew || Crew.Fail(TEXT("startRoles"), TEXT("more workers than the crew maximum"));
	}

	bool ParseWorks(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		return ForEachEntry(Root, TEXT("works"), Error, [&Balance](FName Id, const FReader& Reader)
		{
			FCytraWorkDef Def;
			Def.Id = Id;
			FName On;
			if (!Reader.Name(TEXT("on"), On) || !Reader.Name(TEXT("role"), Def.Role) || !CheckRole(Balance, Reader, TEXT("role"), Def.Role)
				|| !Reader.Integer(TEXT("cost"), Def.Cost) || !CheckAtLeast(Reader, TEXT("cost"), Def.Cost, 0)
				|| !Reader.Number(TEXT("time"), Def.Time) || !CheckPositive(Reader, TEXT("time"), Def.Time))
			{
				return false;
			}
			const int64 Type = StaticEnum<ECytraTileType>()->GetValueByName(On);
			if (Type == INDEX_NONE || !IsCytraOpenable(static_cast<ECytraTileType>(Type)))
			{
				return Reader.Fail(TEXT("on"), FString::Printf(TEXT("'%s' is not a tile type a job can open"), *On.ToString()));
			}
			Def.On = static_cast<ECytraTileType>(Type);
			Balance.Works.Add(Id, Def);
			return true;
		});
	}

	bool ParseTowers(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		return ForEachEntry(Root, TEXT("towers"), Error, [&Balance](FName Id, const FReader& Reader)
		{
			FCytraTowerDef Def;
			Def.Id = Id;
			FName Behavior;
			if (!Reader.Name(TEXT("behavior"), Behavior) || !Reader.Name(TEXT("role"), Def.Role) || !CheckRole(Balance, Reader, TEXT("role"), Def.Role)
				|| !Reader.Integer(TEXT("cost"), Def.Cost) || !CheckAtLeast(Reader, TEXT("cost"), Def.Cost, 0)
				|| !Reader.Number(TEXT("time"), Def.Time) || !CheckPositive(Reader, TEXT("time"), Def.Time)
				|| !Reader.Number(TEXT("range"), Def.Range) || !CheckPositive(Reader, TEXT("range"), Def.Range)
				|| !Reader.Number(TEXT("damage"), Def.Damage) || !CheckAtLeast(Reader, TEXT("damage"), Def.Damage, 0)
				|| !Reader.Number(TEXT("rate"), Def.Rate) || !CheckPositive(Reader, TEXT("rate"), Def.Rate))
			{
				return false;
			}

			if (Behavior == TEXT("Bolt"))
			{
				Def.Behavior = ECytraTowerBehavior::Bolt;
				if (!Reader.Number(TEXT("boltSpeed"), Def.BoltSpeed) || !CheckPositive(Reader, TEXT("boltSpeed"), Def.BoltSpeed))
				{
					return false;
				}
			}
			else if (Behavior == TEXT("Shell"))
			{
				Def.Behavior = ECytraTowerBehavior::Shell;
				if (!Reader.Number(TEXT("splash"), Def.Splash) || !CheckPositive(Reader, TEXT("splash"), Def.Splash)
					|| !Reader.Number(TEXT("flightTime"), Def.FlightTime) || !CheckPositive(Reader, TEXT("flightTime"), Def.FlightTime))
				{
					return false;
				}
			}
			else if (Behavior == TEXT("Chain"))
			{
				Def.Behavior = ECytraTowerBehavior::Chain;
				if (!Reader.Integer(TEXT("targets"), Def.Targets) || !CheckAtLeast(Reader, TEXT("targets"), Def.Targets, 1)
					|| !Reader.Number(TEXT("slow"), Def.Slow) || !CheckPositive(Reader, TEXT("slow"), Def.Slow)
					|| !Reader.Number(TEXT("slowTime"), Def.SlowTime) || !CheckAtLeast(Reader, TEXT("slowTime"), Def.SlowTime, 0))
				{
					return false;
				}
			}
			else
			{
				return Reader.Fail(TEXT("behavior"), FString::Printf(TEXT("unknown behavior '%s' (Bolt, Shell or Chain)"), *Behavior.ToString()));
			}
			Balance.Towers.Add(Id, Def);
			return true;
		});
	}

	bool ParseEnemies(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		return ForEachEntry(Root, TEXT("enemies"), Error, [&Balance](FName Id, const FReader& Reader)
		{
			FCytraEnemyDef Def;
			Def.Id = Id;
			if (!Reader.Integer(TEXT("hp"), Def.Hp) || !CheckAtLeast(Reader, TEXT("hp"), Def.Hp, 1)
				|| !Reader.Number(TEXT("speed"), Def.Speed) || !CheckPositive(Reader, TEXT("speed"), Def.Speed)
				|| !Reader.Integer(TEXT("gold"), Def.Gold) || !CheckAtLeast(Reader, TEXT("gold"), Def.Gold, 0)
				|| !Reader.Integer(TEXT("eggs"), Def.Eggs) || !CheckAtLeast(Reader, TEXT("eggs"), Def.Eggs, 0))
			{
				return false;
			}
			Balance.Enemies.Add(Id, Def);
			return true;
		});
	}

	bool ParseUpgradeAndDismantle(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		TSharedPtr<FJsonObject> UpgradeObject;
		TSharedPtr<FJsonObject> DismantleObject;
		if (!Root.Object(TEXT("upgrade"), UpgradeObject) || !Root.Object(TEXT("dismantle"), DismantleObject))
		{
			return false;
		}

		const FReader Upgrade(*UpgradeObject, TEXT("upgrade"), Error);
		FCytraUpgradeDef& Def = Balance.Upgrade;
		if (!Upgrade.Name(TEXT("role"), Def.Role) || !CheckRole(Balance, Upgrade, TEXT("role"), Def.Role)
			|| !Upgrade.Number(TEXT("time"), Def.Time) || !CheckPositive(Upgrade, TEXT("time"), Def.Time)
			|| !Upgrade.Integer(TEXT("maxLevel"), Def.MaxLevel) || !CheckAtLeast(Upgrade, TEXT("maxLevel"), Def.MaxLevel, 1)
			|| !Upgrade.Number(TEXT("damagePerLevel"), Def.DamagePerLevel)
			|| !Upgrade.Number(TEXT("rangePerLevel"), Def.RangePerLevel)
			|| !Upgrade.Number(TEXT("costFactor"), Def.CostFactor) || !CheckAtLeast(Upgrade, TEXT("costFactor"), Def.CostFactor, 0)
			|| !Upgrade.Integer(TEXT("costRounding"), Def.CostRounding) || !CheckAtLeast(Upgrade, TEXT("costRounding"), Def.CostRounding, 1))
		{
			return false;
		}

		const FReader Dismantle(*DismantleObject, TEXT("dismantle"), Error);
		return Dismantle.Number(TEXT("time"), Balance.DismantleTime) && CheckPositive(Dismantle, TEXT("time"), Balance.DismantleTime)
			&& Dismantle.Number(TEXT("refund"), Balance.DismantleRefund) && CheckAtLeast(Dismantle, TEXT("refund"), Balance.DismantleRefund, 0);
	}

	bool ParseMineWavesTargeting(const FReader& Root, FCytraBalance& Balance, FString& Error)
	{
		TSharedPtr<FJsonObject> MineObject;
		TSharedPtr<FJsonObject> WavesObject;
		TSharedPtr<FJsonObject> TargetingObject;
		if (!Root.Object(TEXT("mine"), MineObject) || !Root.Object(TEXT("waves"), WavesObject) || !Root.Object(TEXT("targeting"), TargetingObject)
			|| !Root.Number(TEXT("highSlotRangeBonus"), Balance.HighSlotRangeBonus) || !CheckPositive(Root, TEXT("highSlotRangeBonus"), Balance.HighSlotRangeBonus))
		{
			return false;
		}

		const FReader Mine(*MineObject, TEXT("mine"), Error);
		const FReader Waves(*WavesObject, TEXT("waves"), Error);
		const FReader Targeting(*TargetingObject, TEXT("targeting"), Error);
		return Mine.Integer(TEXT("slots"), Balance.MineSlots) && CheckAtLeast(Mine, TEXT("slots"), Balance.MineSlots, 0)
			&& Mine.Number(TEXT("period"), Balance.MinePeriod) && CheckPositive(Mine, TEXT("period"), Balance.MinePeriod)
			&& Mine.Integer(TEXT("yield"), Balance.MineYield) && CheckAtLeast(Mine, TEXT("yield"), Balance.MineYield, 0)
			&& Waves.Number(TEXT("hpScalePerWave"), Balance.HpScalePerWave)
			&& Waves.Integer(TEXT("endBonusBase"), Balance.EndBonusBase)
			&& Waves.Integer(TEXT("endBonusPerWave"), Balance.EndBonusPerWave)
			&& Waves.Number(TEXT("prepBase"), Balance.PrepBase)
			&& Waves.Number(TEXT("prepScale"), Balance.PrepScale)
			&& Waves.Integer(TEXT("threatLevels"), Balance.ThreatLevels) && CheckAtLeast(Waves, TEXT("threatLevels"), Balance.ThreatLevels, 1)
			&& Waves.Integer(TEXT("earlyLaunchGoldPerSecond"), Balance.EarlyLaunchGoldPerSecond)
			&& Targeting.Number(TEXT("minX"), Balance.TargetingMinX);
	}
}

bool FCytraBalanceLoader::ParseJson(const FString& Json, FCytraBalance& OutBalance, FString& OutError)
{
	OutBalance = FCytraBalance();

	TSharedPtr<FJsonObject> Root;
	if (!CytraJson::ParseObject(Json, Root, OutError))
	{
		return false;
	}

	const FReader Reader(*Root, FString(), OutError);
	return ParseStart(Reader, OutBalance, OutError)
		&& ParseRoles(Reader, OutBalance, OutError)
		&& ParseCrew(Reader, OutBalance, OutError)
		&& ParseWorks(Reader, OutBalance, OutError)
		&& ParseTowers(Reader, OutBalance, OutError)
		&& ParseEnemies(Reader, OutBalance, OutError)
		&& ParseUpgradeAndDismantle(Reader, OutBalance, OutError)
		&& ParseMineWavesTargeting(Reader, OutBalance, OutError);
}

bool FCytraBalanceLoader::LoadFromFile(const FString& Path, FCytraBalance& OutBalance, FString& OutError)
{
	FString Json;
	if (!CytraJson::LoadFile(Path, Json, OutError))
	{
		return false;
	}
	if (!ParseJson(Json, OutBalance, OutError))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Path, *OutError);
		return false;
	}
	return true;
}
