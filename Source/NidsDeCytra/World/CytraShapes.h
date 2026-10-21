// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Helpers for the placeholder visuals built from engine basic shapes. */
namespace CytraShapes
{
	/** Engine basic shapes are 100 units wide, centred on their origin. */
	constexpr float Size = 100.0f;

	/** Color from an sRGB hex string such as "a9814e". */
	inline FLinearColor FromHex(const TCHAR* Hex)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	}
}
