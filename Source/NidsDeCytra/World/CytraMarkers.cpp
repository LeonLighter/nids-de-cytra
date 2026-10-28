// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraMarkers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rules/CytraCrew.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraBoard.h"
#include "World/CytraShapes.h"

namespace
{
	// Selection frame of the web prototype, converted from its 40-pixel cells: a 2.5-pixel line, 1.5 pixels inside the cell.
	constexpr float FrameInset = 1.5f / 40.0f;
	constexpr float FrameWidth = 2.5f / 40.0f;
	// Hover frame: a 1.5-pixel line whose middle is 1 pixel inside the cell, half-transparent white over the tile.
	constexpr float HoverInset = 0.25f / 40.0f;
	constexpr float HoverWidth = 1.5f / 40.0f;
	const TCHAR* const HoverColor = TEXT("b9bfc8");
	/** World units above the tile top. */
	constexpr float FrameLift = 3.0f;
	constexpr float FrameThickness = 4.0f;

	// Job bar of the web prototype: a 32 x 6 pixel back and a 30 x 4 fill, centred 18 pixels above the cell centre.
	constexpr float BarOffset = 18.0f / 40.0f;
	constexpr float BarBackLength = 32.0f / 40.0f;
	constexpr float BarBackWidth = 6.0f / 40.0f;
	constexpr float BarFillLength = 30.0f / 40.0f;
	constexpr float BarFillWidth = 4.0f / 40.0f;
	constexpr float DotSize = 3.0f / 40.0f;
	constexpr float DotSpacing = 6.0f / 40.0f;
	/** World height of the bars: above the highest tile, so nothing hides them in the top view. */
	constexpr float BarHeight = 100.0f;
}

ACytraMarkers::ACytraMarkers()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;
}

UInstancedStaticMeshComponent* ACytraMarkers::MakeBoxes(FName Name, const TCHAR* Hex)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Name);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetStaticMesh(CubeMesh);
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
		Material->SetVectorParameterValue(TEXT("Color"), CytraShapes::FromHex(Hex));
		Component->SetMaterial(0, Material);
	}
	Component->RegisterComponent();
	return Component;
}

void ACytraMarkers::ShowJobs(const ACytraBoard& Board, const TArray<FCytraJob>& Jobs)
{
	if (!BarBacks)
	{
		BarBacks = MakeBoxes(TEXT("BarBacks"), TEXT("10141b"));
		BarFills = MakeBoxes(TEXT("BarFills"), TEXT("f0a04b"));
		WaitingMarks = MakeBoxes(TEXT("WaitingMarks"), TEXT("9ca8b8"));
	}
	BarBacks->ClearInstances();
	BarFills->ClearInstances();
	WaitingMarks->ClearInstances();

	const float Cell = ACytraBoard::CellSize;
	const float Thin = 2.0f / CytraShapes::Size;
	for (const FCytraJob& Job : Jobs)
	{
		// North of the cell centre is world +X.
		const FVector Center = Board.CellToWorld(Job.Cell) + FVector(BarOffset * Cell, 0.0f, BarHeight);
		BarBacks->AddInstance(FTransform(FQuat::Identity, Center, FVector(BarBackWidth * Cell / CytraShapes::Size, BarBackLength * Cell / CytraShapes::Size, Thin)), true);

		const float Ratio = Job.Time > 0.0 ? static_cast<float>(FMath::Min(1.0, Job.Progress / Job.Time)) : 1.0f;
		if (Ratio > 0.0f)
		{
			// The fill grows eastward from the west end of the bar.
			const float Length = BarFillLength * Ratio * Cell;
			const FVector FillCenter = Center + FVector(0.0f, (Length - BarFillLength * Cell) * 0.5f, 2.0f);
			UInstancedStaticMeshComponent* Fill = Job.Worker != 0 ? BarFills.Get() : WaitingMarks.Get();
			Fill->AddInstance(FTransform(FQuat::Identity, FillCenter, FVector(BarFillWidth * Cell / CytraShapes::Size, Length / CytraShapes::Size, Thin)), true);
		}
		if (Job.Worker == 0)
		{
			for (int32 i = -1; i <= 1; ++i)
			{
				const FVector Dot = Center + FVector(0.0f, i * DotSpacing * Cell, 4.0f);
				WaitingMarks->AddInstance(FTransform(FQuat::Identity, Dot, FVector(DotSize * Cell / CytraShapes::Size, DotSize * Cell / CytraShapes::Size, Thin)), true);
			}
		}
	}
}

void ACytraMarkers::AddFrame(UInstancedStaticMeshComponent& Boxes, const ACytraBoard& Board, const FIntPoint& Cell, float Inset, float FrameLine)
{
	const float CellSize = ACytraBoard::CellSize;
	const FVector Center = Board.CellToWorld(Cell) + FVector(0.0f, 0.0f, Board.GetTileHeight(Board.GetMap().Get(Cell)) + FrameLift);
	// Distance from the cell centre to the middle of each side of the frame.
	const float Offset = CellSize * (0.5f - Inset - FrameLine * 0.5f);
	const float Length = CellSize * (1.0f - 2.0f * Inset) / CytraShapes::Size;
	const float Width = CellSize * FrameLine / CytraShapes::Size;
	const float Thickness = FrameThickness / CytraShapes::Size;

	// Board world axes: grid x along +Y, grid y along -X.
	Boxes.AddInstance(FTransform(FQuat::Identity, Center + FVector(Offset, 0.0f, 0.0f), FVector(Width, Length, Thickness)), true);
	Boxes.AddInstance(FTransform(FQuat::Identity, Center - FVector(Offset, 0.0f, 0.0f), FVector(Width, Length, Thickness)), true);
	Boxes.AddInstance(FTransform(FQuat::Identity, Center + FVector(0.0f, Offset, 0.0f), FVector(Length, Width, Thickness)), true);
	Boxes.AddInstance(FTransform(FQuat::Identity, Center - FVector(0.0f, Offset, 0.0f), FVector(Length, Width, Thickness)), true);
}

void ACytraMarkers::ShowSelection(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell)
{
	if (!SelectionFrame)
	{
		SelectionFrame = MakeBoxes(TEXT("SelectionFrame"), TEXT("f0a04b"));
	}
	SelectionFrame->ClearInstances();
	if (Cell)
	{
		AddFrame(*SelectionFrame, Board, *Cell, FrameInset, FrameWidth);
	}
}

void ACytraMarkers::ShowHover(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell)
{
	if (!HoverFrame)
	{
		HoverFrame = MakeBoxes(TEXT("HoverFrame"), HoverColor);
	}
	HoverFrame->ClearInstances();
	if (Cell)
	{
		AddFrame(*HoverFrame, Board, *Cell, HoverInset, HoverWidth);
	}
}

void ACytraMarkers::ShowNextRoutes(const ACytraBoard& Board, const TArray<int32>& Routes, float Pulse)
{
	// Arrows of the web prototype, in its 40-pixel cells: two chevrons 9 pixels long and 18 high, 12 pixels apart,
	// the first one 10 to 16 pixels from the west edge of the map, so 0.75 to 0.9 cell past the first waypoint.
	constexpr float FirstArrow = 0.75f;
	constexpr float PulseShift = 6.0f / 40.0f;
	constexpr float ArrowSpacing = 12.0f / 40.0f;
	constexpr float ArrowLength = 9.0f / 40.0f;
	constexpr float ArrowHalfHeight = 9.0f / 40.0f;
	constexpr float ArrowLine = 3.5f / 40.0f;
	const TCHAR* const RoadColor = TEXT("6d6351");
	const TCHAR* const ArrowColor = TEXT("e2695b");

	if (!RouteArrows)
	{
		RouteArrows = MakeBoxes(TEXT("RouteArrows"), ArrowColor);
		RouteArrowMaterial = Cast<UMaterialInstanceDynamic>(RouteArrows->GetMaterial(0));
	}
	RouteArrows->ClearInstances();
	if (Routes.IsEmpty())
	{
		return;
	}
	// The prototype fades the arrows from half to full opacity over the road: the material is opaque, so blend the colors.
	if (RouteArrowMaterial)
	{
		const FLinearColor Color = FMath::Lerp(CytraShapes::FromHex(RoadColor), CytraShapes::FromHex(ArrowColor), 0.5f + 0.5f * Pulse);
		RouteArrowMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	const float CellSize = ACytraBoard::CellSize;
	const float Height = Board.GetTileHeight(ECytraTileType::Road) + FrameLift;
	const TArray<FCytraRoute>& MapRoutes = Board.GetMap().Routes;
	for (const int32 RouteIndex : Routes)
	{
		if (!MapRoutes.IsValidIndex(RouteIndex) || MapRoutes[RouteIndex].Waypoints.Num() < 2)
		{
			continue;
		}
		const TArray<FIntPoint>& Waypoints = MapRoutes[RouteIndex].Waypoints;
		const FVector2d Start(Waypoints[0].X, Waypoints[0].Y);
		const FVector2d Forward = (FVector2d(Waypoints[1].X, Waypoints[1].Y) - Start).GetSafeNormal();
		const FVector2d Side(-Forward.Y, Forward.X);
		for (int32 Arrow = 0; Arrow < 2; ++Arrow)
		{
			const FVector2d Back = Start + Forward * (FirstArrow + Pulse * PulseShift + Arrow * ArrowSpacing);
			const FVector Tip = Board.GridToWorld(Back + Forward * ArrowLength) + FVector(0.0f, 0.0f, Height);
			CytraShapes::AddSegment(*RouteArrows, Board.GridToWorld(Back - Side * ArrowHalfHeight) + FVector(0.0f, 0.0f, Height), Tip, ArrowLine * CellSize, FrameThickness);
			CytraShapes::AddSegment(*RouteArrows, Board.GridToWorld(Back + Side * ArrowHalfHeight) + FVector(0.0f, 0.0f, Height), Tip, ArrowLine * CellSize, FrameThickness);
		}
	}
}

void ACytraMarkers::ShowRange(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell, double Radius, bool bDashed)
{
	// Range circle of the web prototype: a 1.5-pixel orange line, dashed 6 on 5 pixels for a tower not built yet.
	constexpr float LineWidth = 1.5f / 40.0f;
	constexpr int32 Segments = 64;
	/** Dash length of about 6 pixels: the number of segments follows the circumference. */
	constexpr float DashLength = 6.0f / 40.0f;

	if (!RangeRing)
	{
		RangeRing = MakeBoxes(TEXT("RangeRing"), TEXT("f0a04b"));
	}
	RangeRing->ClearInstances();
	if (!Cell || Radius <= 0.0)
	{
		return;
	}
	const float CellSize = ACytraBoard::CellSize;
	const float RadiusWorld = static_cast<float>(Radius) * CellSize;
	const int32 Count = bDashed ? FMath::Max(16, 2 * FMath::RoundToInt32(PI * static_cast<float>(Radius) / DashLength)) : Segments;
	// Above the rock, the highest tile, so that the circle reads over the whole board as in the prototype.
	const FVector Center = Board.CellToWorld(*Cell) + FVector(0.0f, 0.0f, Board.GetTileHeight(ECytraTileType::Rock) + FrameLift);
	CytraShapes::AddRing(*RangeRing, Center, RadiusWorld, LineWidth * CellSize, FrameThickness, Count, bDashed);
}
