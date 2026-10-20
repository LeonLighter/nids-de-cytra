// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CytraTileType.generated.h"

/** What a grid cell is. Map files name these values in their legend. */
UENUM()
enum class ECytraTileType : uint8
{
	Rock,
	/** Enemy road, walkable by workers. */
	Road,
	/** Gallery or ledge reserved for workers. */
	Ledge,
	/** Tower slot. */
	Slot,
	/** Tower slot on a promontory (range bonus). */
	HighSlot,
	Rubble,
	Gap,
	Cliff,
	/** Main camp. */
	Camp,
	OutpostSite,
	Mine,
	/** Last cell of every route. */
	Nest,
};

/** A tile that can hold a tower. */
inline bool IsCytraSlot(ECytraTileType Type)
{
	return Type == ECytraTileType::Slot || Type == ECytraTileType::HighSlot;
}

/** A tile that blocks workers until a job opens it. */
inline bool IsCytraObstacle(ECytraTileType Type)
{
	return Type == ECytraTileType::Rubble || Type == ECytraTileType::Gap || Type == ECytraTileType::Cliff;
}

/** A tile that a job can open once: obstacles, the mine and the outpost site. */
inline bool IsCytraOpenable(ECytraTileType Type)
{
	return IsCytraObstacle(Type) || Type == ECytraTileType::Mine || Type == ECytraTileType::OutpostSite;
}
