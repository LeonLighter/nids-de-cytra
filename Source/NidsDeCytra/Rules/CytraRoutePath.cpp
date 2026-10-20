// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraRoutePath.h"

#include "Rules/CytraMapDef.h"

FCytraRoutePath::FCytraRoutePath(const FCytraRoute& Route)
	: Id(Route.Id)
{
	check(Route.Waypoints.Num() >= 2);
	for (int32 i = 0; i + 1 < Route.Waypoints.Num(); ++i)
	{
		const FIntPoint From = Route.Waypoints[i];
		const FIntPoint To = Route.Waypoints[i + 1];
		const double SegmentLength = FMath::Abs(To.X - From.X) + FMath::Abs(To.Y - From.Y);
		Segments.Add({ FVector2d(From.X, From.Y), FVector2d(To.X, To.Y), Length, SegmentLength });
		Length += SegmentLength;
	}
}

FVector2d FCytraRoutePath::PositionAt(double Distance) const
{
	if (Distance <= 0.0)
	{
		return Segments[0].From;
	}
	for (const FSegment& Segment : Segments)
	{
		if (Distance <= Segment.Start + Segment.Length)
		{
			const double Alpha = (Distance - Segment.Start) / Segment.Length;
			return Segment.From + (Segment.To - Segment.From) * Alpha;
		}
	}
	return Segments.Last().To;
}
