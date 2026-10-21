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

void ACytraMarkers::ShowSelection(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell)
{
	if (!SelectionFrame)
	{
		SelectionFrame = MakeBoxes(TEXT("SelectionFrame"), TEXT("f0a04b"));
	}
	SelectionFrame->ClearInstances();
	if (!Cell)
	{
		return;
	}

	const float CellSize = ACytraBoard::CellSize;
	const FVector Center = Board.CellToWorld(*Cell) + FVector(0.0f, 0.0f, Board.GetTileHeight(Board.GetMap().Get(*Cell)) + FrameLift);
	// Distance from the cell centre to the middle of each side of the frame.
	const float Offset = CellSize * (0.5f - FrameInset - FrameWidth * 0.5f);
	const float Length = CellSize * (1.0f - 2.0f * FrameInset) / CytraShapes::Size;
	const float Width = CellSize * FrameWidth / CytraShapes::Size;
	const float Thickness = FrameThickness / CytraShapes::Size;

	// Board world axes: grid x along +Y, grid y along -X.
	SelectionFrame->AddInstance(FTransform(FQuat::Identity, Center + FVector(Offset, 0.0f, 0.0f), FVector(Width, Length, Thickness)), true);
	SelectionFrame->AddInstance(FTransform(FQuat::Identity, Center - FVector(Offset, 0.0f, 0.0f), FVector(Width, Length, Thickness)), true);
	SelectionFrame->AddInstance(FTransform(FQuat::Identity, Center + FVector(0.0f, Offset, 0.0f), FVector(Length, Width, Thickness)), true);
	SelectionFrame->AddInstance(FTransform(FQuat::Identity, Center - FVector(0.0f, Offset, 0.0f), FVector(Length, Width, Thickness)), true);
}
