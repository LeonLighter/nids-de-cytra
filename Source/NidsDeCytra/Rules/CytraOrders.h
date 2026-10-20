// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCytraGrid;
struct FCytraBalance;

enum class ECytraOrderKind : uint8
{
	/** A job from the balance "works" list, which opens the tile. */
	Work,
	Build,
	Upgrade,
	Dismantle,
};

/** What the player asks for on a cell. Id names the work or the tower type; it is unused for upgrades and dismantling. */
struct FCytraOrderAction
{
	ECytraOrderKind Kind = ECytraOrderKind::Work;
	FName Id;

	static FCytraOrderAction Work(FName WorkId) { return { ECytraOrderKind::Work, WorkId }; }
	static FCytraOrderAction Build(FName TowerType) { return { ECytraOrderKind::Build, TowerType }; }
	static FCytraOrderAction Upgrade() { return { ECytraOrderKind::Upgrade, NAME_None }; }
	static FCytraOrderAction Dismantle() { return { ECytraOrderKind::Dismantle, NAME_None }; }
};

/** The job an order would create. */
struct FCytraOrderSpec
{
	ECytraOrderKind Kind = ECytraOrderKind::Work;
	/** Work id or tower type. */
	FName Id;
	FName Role;
	int32 Cost = 0;
	double Time = 0.0;
	/** Gold given back at the end of a dismantling. */
	int32 Refund = 0;
};

/** Why an order is refused, in the order the checks run. */
enum class ECytraOrderRefusal : uint8
{
	None,
	/** The action does not apply to this cell. */
	NotAvailableHere,
	GameOver,
	JobInProgress,
	/** "Open an access first." */
	Unreachable,
	/** No worker of the required specialty in the crew. */
	MissingSpecialty,
	NotEnoughGold,
};

/** State outside the grid that an order depends on. The crew and the job queue provide it in later milestones. */
struct FCytraOrderContext
{
	bool bGameOver = false;
	bool bJobOnCell = false;
	/** Specialty of every worker in the crew. */
	TArray<FName> CrewRoles;
	int32 Gold = 0;
};

struct FCytraOrderCheck
{
	ECytraOrderRefusal Refusal = ECytraOrderRefusal::None;
	/** Set whenever the action applies to the cell, even if refused for another reason. */
	TOptional<FCytraOrderSpec> Spec;
	int32 MissingGold = 0;

	bool IsAccepted() const { return Refusal == ECytraOrderRefusal::None; }
};

struct NIDSDECYTRA_API FCytraOrders
{
	/** The job the action would create on Cell, or unset if the action does not apply there. */
	static TOptional<FCytraOrderSpec> Describe(const FCytraGrid& Grid, const FCytraBalance& Balance, const FIntPoint& Cell, const FCytraOrderAction& Action);

	/** Whether the order is accepted now, or the first reason it is refused. */
	static FCytraOrderCheck Check(const FCytraGrid& Grid, const FCytraBalance& Balance, const FIntPoint& Cell, const FCytraOrderAction& Action, const FCytraOrderContext& Context);
};
