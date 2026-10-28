// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UInstancedStaticMeshComponent;

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

	/**
	 * Adds a flat horizontal ring of cube instances around Center, in world units.
	 * With bDashed, every other segment is left out.
	 */
	void AddRing(UInstancedStaticMeshComponent& Boxes, const FVector& Center, float Radius, float Width, float Thickness, int32 Segments, bool bDashed = false);

	/** Adds a flat cube instance from From to To, Width wide and Thickness thick, in world units. */
	void AddSegment(UInstancedStaticMeshComponent& Boxes, const FVector& From, const FVector& To, float Width, float Thickness);
}
