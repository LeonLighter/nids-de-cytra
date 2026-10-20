// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace CytraMath
{
	/** Rounds halves up, exactly like JavaScript's Math.round used by the prototype. */
	inline int32 Round(double Value)
	{
		return static_cast<int32>(FMath::FloorToDouble(Value + 0.5));
	}
}
