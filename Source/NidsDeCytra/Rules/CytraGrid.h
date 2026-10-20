// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraMapDef.h"

/** A tower standing on a slot. */
struct FCytraTower
{
	FName Type;
	int32 Level = 1;
	/** Build cost plus every upgrade paid, the base of the dismantling refund. */
	int32 Invested = 0;
};

/**
 * Live state of the map: opened tiles and towers, and the area workers can reach from the main camp.
 * Workers move between the 4 neighbouring cells; paths are shortest paths (breadth-first search).
 */
class NIDSDECYTRA_API FCytraGrid
{
public:
	explicit FCytraGrid(const FCytraMapDef& InMap);

	const FCytraMapDef& GetMap() const { return Map; }
	ECytraTileType GetType(const FIntPoint& Cell) const { return Map.Get(Cell); }

	/** True once a job has cleared the obstacle, opened the mine or raised the outpost on this cell. */
	bool IsOpened(const FIntPoint& Cell) const;

	/** Marks an openable cell as opened and updates the reachable area. */
	void Open(const FIntPoint& Cell);

	const FCytraTower* GetTower(const FIntPoint& Cell) const;
	/** Puts a level 1 tower on a free slot. */
	void PlaceTower(const FIntPoint& Cell, FName Type, int32 Cost);
	/** Raises the tower on Cell by one level. */
	void UpgradeTower(const FIntPoint& Cell, int32 Cost);
	/** Frees the slot and returns the tower that stood on it. */
	FCytraTower RemoveTower(const FIntPoint& Cell);

	bool IsWalkable(const FIntPoint& Cell) const;
	/** The main camp, or an outpost once raised. */
	bool IsCamp(const FIntPoint& Cell) const;

	/** In the area connected to the main camp by walkable cells. */
	bool IsReachable(const FIntPoint& Cell) const;
	/** Has a reachable neighbour, so a worker can stand next to it. */
	bool IsAdjacentToReach(const FIntPoint& Cell) const;
	int32 ReachableSlotCount() const;

	/**
	 * Obstacles to open, in order from the reachable area, to give workers access to Target.
	 * Empty if Target is already reachable or cannot be reached at all.
	 */
	TArray<FIntPoint> AccessChain(const FIntPoint& Target) const;

	/**
	 * Shortest path from Start to the first cell that satisfies Goal, over walkable cells.
	 * The path excludes Start and includes the goal cell; it is empty when Start already satisfies Goal.
	 * Unset when no cell satisfying Goal can be reached.
	 */
	TOptional<TArray<FIntPoint>> FindPath(const FIntPoint& Start, TFunctionRef<bool(const FIntPoint&)> Goal) const;

	/** Same, but cells are passable when CanPass says so. */
	TOptional<TArray<FIntPoint>> FindPath(const FIntPoint& Start, TFunctionRef<bool(const FIntPoint&)> Goal, TFunctionRef<bool(const FIntPoint&)> CanPass) const;

	/** Neighbour order of every search, as in the prototype: east, west, south, north. */
	static const FIntPoint Directions[4];

private:
	int32 Index(const FIntPoint& Cell) const { return Cell.Y * Map.Cols + Cell.X; }
	void ComputeReach();

	FCytraMapDef Map;
	TBitArray<> Opened;
	TBitArray<> Reach;
	TMap<FIntPoint, FCytraTower> Towers;
};
