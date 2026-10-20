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
