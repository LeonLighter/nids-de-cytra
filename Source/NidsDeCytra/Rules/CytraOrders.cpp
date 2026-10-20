// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraOrders.h"

#include "Rules/CytraBalance.h"
#include "Rules/CytraGrid.h"

TOptional<FCytraOrderSpec> FCytraOrders::Describe(const FCytraGrid& Grid, const FCytraBalance& Balance, const FIntPoint& Cell, const FCytraOrderAction& Action)
{
	if (!Grid.GetMap().IsInside(Cell))
	{
		return {};
	}
	const ECytraTileType Type = Grid.GetType(Cell);
	const FCytraTower* Tower = Grid.GetTower(Cell);

	switch (Action.Kind)
	{
	case ECytraOrderKind::Work:
	{
		const FCytraWorkDef* Work = Balance.Works.Find(Action.Id);
		if (!Work || Work->On != Type || Grid.IsOpened(Cell))
		{
			return {};
		}
		return FCytraOrderSpec{ ECytraOrderKind::Work, Work->Id, Work->Role, Work->Cost, Work->Time };
	}
	case ECytraOrderKind::Build:
	{
		const FCytraTowerDef* Def = Balance.Towers.Find(Action.Id);
		if (!Def || !IsCytraSlot(Type) || Tower)
		{
			return {};
		}
		return FCytraOrderSpec{ ECytraOrderKind::Build, Def->Id, Def->Role, Def->Cost, Def->Time };
	}
	case ECytraOrderKind::Upgrade:
		if (!Tower || Tower->Level >= Balance.Upgrade.MaxLevel)
		{
			return {};
		}
		return FCytraOrderSpec{ ECytraOrderKind::Upgrade, Tower->Type, Balance.Upgrade.Role, Balance.UpgradeCost(Tower->Type, Tower->Level), Balance.Upgrade.Time };
	case ECytraOrderKind::Dismantle:
		if (!Tower)
		{
			return {};
		}
		return FCytraOrderSpec{ ECytraOrderKind::Dismantle, Tower->Type, Balance.Towers.FindChecked(Tower->Type).Role, 0, Balance.DismantleTime,
			Balance.DismantleRefundOf(Tower->Invested) };
	}
	return {};
}

FCytraOrderCheck FCytraOrders::Check(const FCytraGrid& Grid, const FCytraBalance& Balance, const FIntPoint& Cell, const FCytraOrderAction& Action, const FCytraOrderContext& Context)
{
	FCytraOrderCheck Result;
	Result.Spec = Describe(Grid, Balance, Cell, Action);
	if (!Result.Spec)
	{
		Result.Refusal = ECytraOrderRefusal::NotAvailableHere;
		return Result;
	}

	// Workers stand on a slot to build; for any other target they stand on a neighbouring cell.
	const bool bReachable = IsCytraSlot(Grid.GetType(Cell)) ? Grid.IsReachable(Cell) : Grid.IsAdjacentToReach(Cell);

	if (Context.bGameOver)
	{
		Result.Refusal = ECytraOrderRefusal::GameOver;
	}
	else if (Context.bJobOnCell)
	{
		Result.Refusal = ECytraOrderRefusal::JobInProgress;
	}
	else if (!bReachable)
	{
		Result.Refusal = ECytraOrderRefusal::Unreachable;
	}
	else if (!Context.CrewRoles.Contains(Result.Spec->Role))
	{
		Result.Refusal = ECytraOrderRefusal::MissingSpecialty;
	}
	else if (Context.Gold < Result.Spec->Cost)
	{
		Result.Refusal = ECytraOrderRefusal::NotEnoughGold;
		Result.MissingGold = Result.Spec->Cost - Context.Gold;
	}
	return Result;
}
