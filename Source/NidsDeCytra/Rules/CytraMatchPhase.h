// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CytraMatchPhase.generated.h"

UENUM()
enum class ECytraMatchPhase : uint8
{
	/** Between waves. Before wave 1 there is no countdown: the player launches it. */
	Prep,
	Wave,
	Won,
	Lost,
};
