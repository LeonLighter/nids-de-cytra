// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraShapes.h"

#include "Components/InstancedStaticMeshComponent.h"

namespace CytraShapes
{
	void AddRing(UInstancedStaticMeshComponent& Boxes, const FVector& Center, float Radius, float Width, float Thickness, int32 Segments, bool bDashed)
	{
		// Each segment is a chord, a little longer so that neighbours meet.
		const float Length = 2.0f * Radius * FMath::Sin(PI / Segments) + Width;
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			if (bDashed && Index % 2 == 1)
			{
				continue;
			}
			const float Angle = (Index + 0.5f) * 2.0f * PI / Segments;
			const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			const FQuat Rotation(FVector::UpVector, Angle);
			// Local X points outward, so the chord runs along local Y.
			Boxes.AddInstance(FTransform(Rotation, Center + Offset, FVector(Width / Size, Length / Size, Thickness / Size)), true);
		}
	}

	void AddSegment(UInstancedStaticMeshComponent& Boxes, const FVector& From, const FVector& To, float Width, float Thickness)
	{
		const FVector Delta = To - From;
		const float Length = Delta.Size2D();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		const FQuat Rotation(FVector::UpVector, FMath::Atan2(Delta.Y, Delta.X));
		Boxes.AddInstance(FTransform(Rotation, (From + To) * 0.5f, FVector(Length / Size, Width / Size, Thickness / Size)), true);
	}
}
