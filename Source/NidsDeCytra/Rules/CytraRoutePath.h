// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCytraRoute;

/** A route as a polyline: enemies move along it by distance travelled, in cells. */
class NIDSDECYTRA_API FCytraRoutePath
{
public:
	explicit FCytraRoutePath(const FCytraRoute& Route);

	FName GetId() const { return Id; }

	/** Total length, in cells, from the spawn point to the nest. */
	double GetLength() const { return Length; }

	/** Grid position after travelling Distance cells. Clamped to the first and last waypoints. */
	FVector2d PositionAt(double Distance) const;

private:
	struct FSegment
	{
		FVector2d From;
		FVector2d To;
		double Start = 0.0;
		double Length = 0.0;
	};

	FName Id;
	TArray<FSegment> Segments;
	double Length = 0.0;
};
