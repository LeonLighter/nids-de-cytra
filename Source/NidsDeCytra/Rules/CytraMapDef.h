// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraTileType.h"

/** An enemy route: axis-aligned segments between waypoints, in grid cells. The first point may lie off the grid. */
struct FCytraRoute
{
	FName Id;
	TArray<FIntPoint> Waypoints;
};

/** Static description of a map, as read from its data file. X goes east, Y goes south. */
struct NIDSDECYTRA_API FCytraMapDef
{
	FName Id;
	int32 Cols = 0;
	int32 Rows = 0;
	/** Row-major, index = Y * Cols + X. */
	TArray<ECytraTileType> Tiles;
	TArray<FCytraRoute> Routes;

	FIntPoint Camp = FIntPoint(INDEX_NONE);
	FIntPoint Nest = FIntPoint(INDEX_NONE);
	TArray<FIntPoint> Mines;
	TArray<FIntPoint> OutpostSites;

	bool IsInside(const FIntPoint& Cell) const
	{
		return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < Cols && Cell.Y < Rows;
	}

	/** Cells outside the grid read as rock. */
	ECytraTileType Get(const FIntPoint& Cell) const
	{
		return IsInside(Cell) ? Tiles[Cell.Y * Cols + Cell.X] : ECytraTileType::Rock;
	}

	int32 Count(ECytraTileType Type) const;
};
