// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Game/CytraIsoView.h"

namespace
{
	/** The box of the Passe01 board, as ACytraBoard::GetBoardBounds gives it. */
	const FBox BoardBox(FVector(-2700.0, -100.0, 0.0), FVector(100.0, 4700.0, 70.0));

	FCytraScreen MakeScreen(double Width, double Height)
	{
		FCytraScreen Screen;
		Screen.Size = FVector2d(Width, Height);
		Screen.Panel = 360.0;
		Screen.Bar = 96.0;
		return Screen;
	}

	/** Bounds, in pixels, of the board box corners on screen. */
	FBox2d ScreenBox(const FCytraIsoView& View, const FCytraScreen& Screen)
	{
		FBox2d Result(ForceInit);
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point((Corner & 1) ? BoardBox.Max.X : BoardBox.Min.X, (Corner & 2) ? BoardBox.Max.Y : BoardBox.Min.Y, (Corner & 4) ? BoardBox.Max.Z : BoardBox.Min.Z);
			Result += View.WorldToScreen(Point, Screen);
		}
		return Result;
	}

	/** World point of a grid cell centre on Passe01, at ground level. */
	FVector Cell(int32 X, int32 Y)
	{
		return FVector(-Y * 200.0, X * 200.0, 0.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraIsoViewFitsBoardTest, "Cytra.Game.IsoView.FitsBoard", CytraTest::Flags)

bool FCytraIsoViewFitsBoardTest::RunTest(const FString& Parameters)
{
	const FCytraCameraLook Look;
	const FCytraIsoView View(Look, BoardBox);
	TestEqual(TEXT("True isometric pitch"), View.GetRotation().Pitch, -35.264);
	TestEqual(TEXT("Seen from the south-west"), View.GetRotation().Yaw, 45.0);

	for (const FVector2d& Size : { FVector2d(1920.0, 1080.0), FVector2d(1280.0, 720.0), FVector2d(2560.0, 1080.0), FVector2d(1024.0, 768.0) })
	{
		const FCytraScreen Screen = MakeScreen(Size.X, Size.Y);
		const FString What = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
		const FBox2d Box = ScreenBox(View, Screen);
		TestTrue(What + TEXT(": board clear of the panel"), Box.Min.X >= 0.0 && Box.Max.X <= Size.X - Screen.Panel);
		TestTrue(What + TEXT(": board clear of the top bar"), Box.Min.Y >= Screen.Bar && Box.Max.Y <= Size.Y);
		TestTrue(What + TEXT(": board centred in the free area"), Box.GetCenter().Equals(Screen.FreeCenter(), 0.01));
		// The board fills the free area along one side, up to the margin.
		const FVector2d Fill = Box.GetSize() / Screen.FreeSize();
		TestEqual(What + TEXT(": board as large as the margin allows"), FMath::Max(Fill.X, Fill.Y), 1.0 / Look.Margin, 0.001);
	}

	// The west entries show on the left, the nest on the right; north is above south.
	const FCytraScreen Screen = MakeScreen(1920.0, 1080.0);
	TestTrue(TEXT("North entry left of the nest"), View.WorldToScreen(Cell(0, 3), Screen).X < View.WorldToScreen(Cell(22, 7), Screen).X);
	TestTrue(TEXT("South entry left of the nest"), View.WorldToScreen(Cell(0, 12), Screen).X < View.WorldToScreen(Cell(22, 7), Screen).X);
	TestTrue(TEXT("North above south"), View.WorldToScreen(Cell(5, 0), Screen).Y < View.WorldToScreen(Cell(5, 13), Screen).Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraIsoViewZoomAndPanTest, "Cytra.Game.IsoView.ZoomAndPan", CytraTest::Flags)

bool FCytraIsoViewZoomAndPanTest::RunTest(const FString& Parameters)
{
	FCytraCameraLook Look;
	Look.MaxZoom = 3.0;
	FCytraIsoView View(Look, BoardBox);
	const FCytraScreen Screen = MakeScreen(1920.0, 1080.0);
	const FVector2d Middle = View.WorldToScreen(BoardBox.GetCenter(), Screen);

	// The whole board shows: the view does not move.
	View.Pan(FVector2d(500.0, -300.0), Screen);
	TestTrue(TEXT("No panning at the whole board view"), View.WorldToScreen(BoardBox.GetCenter(), Screen).Equals(Middle, 0.01));
	View.ZoomAt(0.5, Middle, Screen);
	TestEqual(TEXT("No zooming out past the whole board"), View.GetZoom(), 1.0);

	// Zooming keeps the point under the cursor in place.
	const FVector Target = Cell(8, 7);
	const FVector2d Cursor = View.WorldToScreen(Target, Screen);
	View.ZoomAt(1.5, Cursor, Screen);
	TestEqual(TEXT("Zoomed in"), View.GetZoom(), 1.5);
	TestTrue(TEXT("The point under the cursor stays there"), View.WorldToScreen(Target, Screen).Equals(Cursor, 0.01));

	View.ZoomAt(10.0, Cursor, Screen);
	TestEqual(TEXT("Closest zoom"), View.GetZoom(), Look.MaxZoom);

	// Panning moves the board under the view, never past its edges.
	const FVector2d Before = View.WorldToScreen(Target, Screen);
	View.Pan(FVector2d(40.0, 0.0), Screen);
	TestTrue(TEXT("Panning right moves the board left"), View.WorldToScreen(Target, Screen).X < Before.X);
	for (const FVector2d& Push : { FVector2d(1e6, 0.0), FVector2d(-1e6, 0.0), FVector2d(0.0, 1e6), FVector2d(0.0, -1e6) })
	{
		View.Pan(Push, Screen);
		const FBox2d Box = ScreenBox(View, Screen);
		const FVector2d Free = Screen.FreeSize();
		TestTrue(FString::Printf(TEXT("Board still covers the free area after a push of (%.0f, %.0f)"), Push.X, Push.Y),
			Box.Min.X <= 0.01 && Box.Max.X >= Free.X - 0.01 && Box.Min.Y <= Screen.Bar + 0.01 && Box.Max.Y >= Screen.Size.Y - 0.01);
	}

	// Back to the whole board, centred again.
	View.ZoomAt(0.01, Cursor, Screen);
	TestEqual(TEXT("Whole board again"), View.GetZoom(), 1.0);
	TestTrue(TEXT("Centred again"), ScreenBox(View, Screen).GetCenter().Equals(Screen.FreeCenter(), 0.01));
	return true;
}

#endif
