// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraGrid.h"

#include "Algo/Reverse.h"

const FIntPoint FCytraGrid::Directions[4] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };

FCytraGrid::FCytraGrid(const FCytraMapDef& InMap)
	: Map(InMap)
	, Opened(false, InMap.Cols * InMap.Rows)
	, Reach(false, InMap.Cols * InMap.Rows)
{
	ComputeReach();
}

bool FCytraGrid::IsOpened(const FIntPoint& Cell) const
{
	return Map.IsInside(Cell) && Opened[Index(Cell)];
}

void FCytraGrid::Open(const FIntPoint& Cell)
{
	check(Map.IsInside(Cell) && IsCytraOpenable(GetType(Cell)));
	Opened[Index(Cell)] = true;
	ComputeReach();
}

const FCytraTower* FCytraGrid::GetTower(const FIntPoint& Cell) const
{
	return Towers.Find(Cell);
}

void FCytraGrid::PlaceTower(const FIntPoint& Cell, FName Type, int32 Cost)
{
	check(IsCytraSlot(GetType(Cell)) && !Towers.Contains(Cell));
	Towers.Add(Cell, FCytraTower{ Type, 1, Cost });
}

void FCytraGrid::UpgradeTower(const FIntPoint& Cell, int32 Cost)
{
	FCytraTower& Tower = Towers.FindChecked(Cell);
	++Tower.Level;
	Tower.Invested += Cost;
}

FCytraTower FCytraGrid::RemoveTower(const FIntPoint& Cell)
{
	return Towers.FindAndRemoveChecked(Cell);
}

bool FCytraGrid::IsWalkable(const FIntPoint& Cell) const
{
	if (!Map.IsInside(Cell))
	{
		return false;
	}
	switch (GetType(Cell))
	{
	case ECytraTileType::Road:
	case ECytraTileType::Ledge:
	case ECytraTileType::Slot:
	case ECytraTileType::HighSlot:
	case ECytraTileType::Camp:
	case ECytraTileType::Nest:
		return true;
	case ECytraTileType::Rubble:
	case ECytraTileType::Gap:
	case ECytraTileType::Cliff:
	case ECytraTileType::OutpostSite:
		return IsOpened(Cell);
	default:
		// Rock, and the mine: miners stand next to it.
		return false;
	}
}

bool FCytraGrid::IsCamp(const FIntPoint& Cell) const
{
	const ECytraTileType Type = GetType(Cell);
	return Map.IsInside(Cell) && (Type == ECytraTileType::Camp || (Type == ECytraTileType::OutpostSite && IsOpened(Cell)));
}

bool FCytraGrid::IsReachable(const FIntPoint& Cell) const
{
	return Map.IsInside(Cell) && Reach[Index(Cell)];
}

bool FCytraGrid::IsAdjacentToReach(const FIntPoint& Cell) const
{
	for (const FIntPoint& Direction : Directions)
	{
		const FIntPoint Neighbour = Cell + Direction;
		if (IsWalkable(Neighbour) && IsReachable(Neighbour))
		{
			return true;
		}
	}
	return false;
}

int32 FCytraGrid::ReachableSlotCount() const
{
	int32 Count = 0;
	for (int32 Y = 0; Y < Map.Rows; ++Y)
	{
		for (int32 X = 0; X < Map.Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			Count += IsCytraSlot(GetType(Cell)) && IsReachable(Cell) ? 1 : 0;
		}
	}
	return Count;
}

void FCytraGrid::ComputeReach()
{
	Reach.Init(false, Map.Cols * Map.Rows);
	TArray<FIntPoint> Queue = { Map.Camp };
	Reach[Index(Map.Camp)] = true;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Cell = Queue[Head];
		for (const FIntPoint& Direction : Directions)
		{
			const FIntPoint Next = Cell + Direction;
			if (IsWalkable(Next) && !Reach[Index(Next)])
			{
				Reach[Index(Next)] = true;
				Queue.Add(Next);
			}
		}
	}
}

TArray<FIntPoint> FCytraGrid::AccessChain(const FIntPoint& Target) const
{
	if (IsReachable(Target))
	{
		return {};
	}

	// Walk from the target back to the reachable area, allowing passage through any obstacle.
	const TOptional<TArray<FIntPoint>> Path = FindPath(Target,
		[this](const FIntPoint& Cell) { return IsReachable(Cell); },
		[this](const FIntPoint& Cell) { return IsWalkable(Cell) || IsCytraObstacle(GetType(Cell)); });
	if (!Path)
	{
		return {};
	}

	TArray<FIntPoint> Chain;
	for (int32 i = Path->Num() - 1; i >= 0; --i)
	{
		const FIntPoint& Cell = (*Path)[i];
		if (IsCytraObstacle(GetType(Cell)) && !IsOpened(Cell))
		{
			Chain.Add(Cell);
		}
	}
	return Chain;
}

TOptional<TArray<FIntPoint>> FCytraGrid::FindPath(const FIntPoint& Start, TFunctionRef<bool(const FIntPoint&)> Goal) const
{
	return FindPath(Start, Goal, [this](const FIntPoint& Cell) { return IsWalkable(Cell); });
}

TOptional<TArray<FIntPoint>> FCytraGrid::FindPath(const FIntPoint& Start, TFunctionRef<bool(const FIntPoint&)> Goal, TFunctionRef<bool(const FIntPoint&)> CanPass) const
{
	if (!Map.IsInside(Start))
	{
		return {};
	}

	// Previous cell index for every visited cell; the start points to itself.
	TArray<int32> Previous;
	Previous.Init(INDEX_NONE, Map.Cols * Map.Rows);
	Previous[Index(Start)] = Index(Start);

	TArray<FIntPoint> Queue = { Start };
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Cell = Queue[Head];
		if (Goal(Cell))
		{
			TArray<FIntPoint> Path;
			for (int32 Current = Index(Cell); Current != Index(Start); Current = Previous[Current])
			{
				Path.Add(FIntPoint(Current % Map.Cols, Current / Map.Cols));
			}
			Algo::Reverse(Path);
			return Path;
		}
		for (const FIntPoint& Direction : Directions)
		{
			const FIntPoint Next = Cell + Direction;
			if (Map.IsInside(Next) && Previous[Index(Next)] == INDEX_NONE && CanPass(Next))
			{
				Previous[Index(Next)] = Index(Cell);
				Queue.Add(Next);
			}
		}
	}
	return {};
}
