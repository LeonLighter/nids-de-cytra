// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraMarkers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
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
