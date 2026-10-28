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

/** Where a map label sits relative to its anchor point. */
enum class ECytraLabelAlign : uint8
{
	/** The text starts at the anchor. */
	Left,
	Center,
	/** The text ends at the anchor. */
	Right,
};

/** A name written on the map, such as a pass or the nest. Its text is Label.<Id> in the string table. */
struct FCytraMapLabel
{
	FName Id;
	/** Anchor in grid cells, at the middle of the text height: cell centres are whole numbers. */
	FVector2d At = FVector2d::ZeroVector;
	ECytraLabelAlign Align = ECytraLabelAlign::Left;
	/** Font size in pixels of the prototype, on its 40-pixel cells. */
	int32 Size = 15;
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
	/** Optional names written on the map. */
	TArray<FCytraMapLabel> Labels;

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
