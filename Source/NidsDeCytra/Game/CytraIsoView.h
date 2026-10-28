// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/CytraLooks.h"

/** The game screen in pixels, and the part of it the interface covers: the top bar and the order panel on the right. */
struct FCytraScreen
{
	FVector2d Size = FVector2d(1920.0, 1080.0);
	double Panel = 0.0;
	double Bar = 0.0;

	/** Size of the area left to the board. */
	FVector2d FreeSize() const;
	/** Centre of the area left to the board, in pixels from the top-left corner. */
	FVector2d FreeCenter() const;
};

/**
 * Orthographic isometric view of the board, with zoom and panning. Pure math, testable without any world:
 * the camera pawn applies it. Positions on the view plane are in world units along the camera right and up axes,
 * from the centre of the board box.
 * At the lowest zoom the whole board fits the free area and the view cannot move; zoomed in, it pans
 * without ever leaving the board.
 */
class NIDSDECYTRA_API FCytraIsoView
{
public:
	FCytraIsoView() = default;
	FCytraIsoView(const FCytraCameraLook& InLook, const FBox& InBounds);

	FRotator GetRotation() const { return Rotation; }
	double GetZoom() const { return Zoom; }

	/** World units per screen pixel. */
	double UnitsPerPixel(const FCytraScreen& Screen) const;
	/** Width of the orthographic view, in world units. */
	double OrthoWidth(const FCytraScreen& Screen) const;
	/** Where the camera stands: far enough back to see the whole box, the focus at the centre of the free area. */
	FVector CameraLocation(const FCytraScreen& Screen) const;
	/** Distance from the camera to the centre of the box, along its view direction. */
	double CameraDistance() const;
	/** The pixel where a world point shows, from the top-left corner of the screen. */
	FVector2d WorldToScreen(const FVector& World, const FCytraScreen& Screen) const;

	/** Moves the view by screen pixels, X to the right and Y up. The view stays over the board. */
	void Pan(const FVector2d& Pixels, const FCytraScreen& Screen);
	/** Multiplies the zoom by Factor, between the whole board and the closest zoom, keeping the board point under the cursor in place. */
	void ZoomAt(double Factor, const FVector2d& Cursor, const FCytraScreen& Screen);
	/** Keeps the view over the board, after a pan, a zoom or a resize. */
	void Clamp(const FCytraScreen& Screen);

private:
	FCytraCameraLook Look;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Center = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;
	/** Half the size of the box seen by the camera, on the view plane. */
	FVector2d Extent = FVector2d::ZeroVector;
	double Radius = 0.0;
	/** The view plane point shown at the centre of the free area. */
	FVector2d Focus = FVector2d::ZeroVector;
	double Zoom = 1.0;
};
