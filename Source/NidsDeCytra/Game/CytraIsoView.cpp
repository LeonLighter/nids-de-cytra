// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraIsoView.h"

FVector2d FCytraScreen::FreeSize() const
{
	return FVector2d(FMath::Max(Size.X - Panel, 1.0), FMath::Max(Size.Y - Bar, 1.0));
}

FVector2d FCytraScreen::FreeCenter() const
{
	return FVector2d((Size.X - Panel) * 0.5, Bar + (Size.Y - Bar) * 0.5);
}

FCytraIsoView::FCytraIsoView(const FCytraCameraLook& InLook, const FBox& InBounds)
	: Look(InLook)
	, Rotation(InLook.Pitch, InLook.Yaw, 0.0)
{
	const FRotationMatrix Axes(Rotation);
	Forward = Axes.GetUnitAxis(EAxis::X);
	Right = Axes.GetUnitAxis(EAxis::Y);
	Up = Axes.GetUnitAxis(EAxis::Z);
	Center = InBounds.GetCenter();
	const FVector Half = InBounds.GetExtent();
	// A box is symmetric about its centre, and so is its projection on the view plane.
	const auto Span = [&Half](const FVector& Axis)
	{
		return FMath::Abs(Axis.X) * Half.X + FMath::Abs(Axis.Y) * Half.Y + FMath::Abs(Axis.Z) * Half.Z;
	};
	Extent = FVector2d(Span(Right), Span(Up));
	Radius = Half.Size();
}

double FCytraIsoView::UnitsPerPixel(const FCytraScreen& Screen) const
{
	const FVector2d Free = Screen.FreeSize();
	const double Fit = FMath::Max(2.0 * Extent.X * Look.Margin / Free.X, 2.0 * Extent.Y * Look.Margin / Free.Y);
	return FMath::Max(Fit, UE_KINDA_SMALL_NUMBER) / Zoom;
}

double FCytraIsoView::OrthoWidth(const FCytraScreen& Screen) const
{
	return Screen.Size.X * UnitsPerPixel(Screen);
}

double FCytraIsoView::CameraDistance() const
{
	return Radius * 2.0 + 1000.0;
}

FVector FCytraIsoView::CameraLocation(const FCytraScreen& Screen) const
{
	// The screen centre is half the panel to the right of the free centre, and half the bar above it.
	const double Scale = UnitsPerPixel(Screen);
	const double AlongRight = Focus.X + Screen.Panel * 0.5 * Scale;
	const double AlongUp = Focus.Y + Screen.Bar * 0.5 * Scale;
	return Center + Right * AlongRight + Up * AlongUp - Forward * CameraDistance();
}

FVector2d FCytraIsoView::WorldToScreen(const FVector& World, const FCytraScreen& Screen) const
{
	const double Scale = UnitsPerPixel(Screen);
	const FVector Offset = World - CameraLocation(Screen);
	return FVector2d(Screen.Size.X * 0.5 + FVector::DotProduct(Offset, Right) / Scale, Screen.Size.Y * 0.5 - FVector::DotProduct(Offset, Up) / Scale);
}

void FCytraIsoView::Pan(const FVector2d& Pixels, const FCytraScreen& Screen)
{
	Focus += Pixels * UnitsPerPixel(Screen);
	Clamp(Screen);
}

void FCytraIsoView::ZoomAt(double Factor, const FVector2d& Cursor, const FCytraScreen& Screen)
{
	const double Before = UnitsPerPixel(Screen);
	Zoom = FMath::Clamp(Zoom * Factor, 1.0, FMath::Max(Look.MaxZoom, 1.0));
	const double After = UnitsPerPixel(Screen);
	// The cursor, in pixels from the free centre, X to the right and Y up.
	const FVector2d FromCenter(Cursor.X - Screen.FreeCenter().X, Screen.FreeCenter().Y - Cursor.Y);
	Focus += FromCenter * (Before - After);
	Clamp(Screen);
}

void FCytraIsoView::Clamp(const FCytraScreen& Screen)
{
	// The free area may not show anything beyond the board: once all of it shows, the focus stays at its centre.
	const FVector2d HalfVisible = Screen.FreeSize() * 0.5 * UnitsPerPixel(Screen);
	const FVector2d Limit(FMath::Max(Extent.X - HalfVisible.X, 0.0), FMath::Max(Extent.Y - HalfVisible.Y, 0.0));
	Focus = FVector2d(FMath::Clamp(Focus.X, -Limit.X, Limit.X), FMath::Clamp(Focus.Y, -Limit.Y, Limit.Y));
}
