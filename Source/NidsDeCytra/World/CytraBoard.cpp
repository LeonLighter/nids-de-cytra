// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraBoard.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "CytraLog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Rules/CytraMapLoader.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName ColorParameter(TEXT("Color"));

	/** Basic shapes are 100 units wide. */
	constexpr float ShapeSize = 100.0f;

	/** Centre of a cell in board space: grid x along +Y, grid y along -X. */
	FVector CellToLocal(const FIntPoint& Cell)
	{
		return FVector(-Cell.Y * ACytraBoard::CellSize, Cell.X * ACytraBoard::CellSize, 0.0f);
	}

	FLinearColor FromHex(const TCHAR* Hex)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	}
}

ACytraBoard::ACytraBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	TileMesh = CubeFinder.Object;
	TileMaterial = MaterialFinder.Object;

	// Placeholder palette, after the web prototype.
	TileColors = {
		{ ECytraTileType::Rock, FromHex(TEXT("2a3340")) },
		{ ECytraTileType::Road, FromHex(TEXT("6d6351")) },
		{ ECytraTileType::Ledge, FromHex(TEXT("9ca8b8")) },
		{ ECytraTileType::Slot, FromHex(TEXT("f0a04b")) },
		{ ECytraTileType::HighSlot, FromHex(TEXT("e9eef4")) },
		{ ECytraTileType::Rubble, FromHex(TEXT("8a8072")) },
		{ ECytraTileType::Gap, FromHex(TEXT("05070a")) },
		{ ECytraTileType::Cliff, FromHex(TEXT("566173")) },
		{ ECytraTileType::Camp, FromHex(TEXT("8cc48d")) },
		{ ECytraTileType::OutpostSite, FromHex(TEXT("8a5a2c")) },
		{ ECytraTileType::Mine, FromHex(TEXT("f2cf63")) },
		{ ECytraTileType::Nest, FromHex(TEXT("bba8ea")) },
	};

	TileHeights = {
		{ ECytraTileType::Rock, 70.0f },
		{ ECytraTileType::Road, 10.0f },
		{ ECytraTileType::Ledge, 16.0f },
		{ ECytraTileType::Slot, 24.0f },
		{ ECytraTileType::HighSlot, 48.0f },
		{ ECytraTileType::Rubble, 36.0f },
		{ ECytraTileType::Gap, 2.0f },
		{ ECytraTileType::Cliff, 56.0f },
		{ ECytraTileType::Camp, 24.0f },
		{ ECytraTileType::OutpostSite, 20.0f },
		{ ECytraTileType::Mine, 36.0f },
		{ ECytraTileType::Nest, 30.0f },
	};
}

void ACytraBoard::BeginPlay()
{
	Super::BeginPlay();
	EnsureBuilt();
}

bool ACytraBoard::EnsureBuilt()
{
	if (bBuilt)
	{
		return true;
	}

	const FString Path = FPaths::ProjectContentDir() / MapFile;
	FString Error;
	if (!FCytraMapLoader::LoadFromFile(Path, Map, Error))
	{
		UE_LOG(LogCytra, Error, TEXT("Board: cannot load map: %s"), *Error);
		return false;
	}
	if (!TileMesh || !TileMaterial)
	{
		UE_LOG(LogCytra, Error, TEXT("Board: engine basic shape or material not found"));
		return false;
	}

	const float Footprint = (CellSize - TileGap) / ShapeSize;
	for (int32 Y = 0; Y < Map.Rows; ++Y)
	{
		for (int32 X = 0; X < Map.Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			const ECytraTileType Type = Map.Get(Cell);
			const float Height = GetTileHeight(Type);
			const FVector Center = CellToLocal(Cell) + FVector(0.0f, 0.0f, Height * 0.5f);
			GetOrCreateTileComponent(Type)->AddInstance(FTransform(FQuat::Identity, Center, FVector(Footprint, Footprint, Height / ShapeSize)));
		}
	}

	bBuilt = true;
	UE_LOG(LogCytra, Log, TEXT("Board: built map %s, %d x %d tiles, %d tile types"), *Map.Id.ToString(), Map.Cols, Map.Rows, TileComponents.Num());
	return true;
}

UInstancedStaticMeshComponent* ACytraBoard::GetOrCreateTileComponent(ECytraTileType Type)
{
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Existing = TileComponents.Find(Type))
	{
		return *Existing;
	}

	const FString TypeName = StaticEnum<ECytraTileType>()->GetNameStringByValue(static_cast<int64>(Type));
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("Tiles_%s"), *TypeName));
	Component->SetupAttachment(RootComponent);
	Component->SetStaticMesh(TileMesh);

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TileMaterial, Component);
	const FLinearColor* Color = TileColors.Find(Type);
	if (!Color)
	{
		UE_LOG(LogCytra, Warning, TEXT("Board: no color for tile type %s"), *TypeName);
	}
	Material->SetVectorParameterValue(ColorParameter, Color ? *Color : FLinearColor::White);
	Component->SetMaterial(0, Material);

	Component->RegisterComponent();
	TileComponents.Add(Type, Component);
	return Component;
}

float ACytraBoard::GetTileHeight(ECytraTileType Type) const
{
	const float* Height = TileHeights.Find(Type);
	return Height ? FMath::Max(*Height, 1.0f) : 10.0f;
}

FVector ACytraBoard::CellToWorld(const FIntPoint& Cell) const
{
	return GetActorTransform().TransformPosition(CellToLocal(Cell));
}

FBox ACytraBoard::GetBoardBounds() const
{
	if (!bBuilt)
	{
		return FBox(ForceInit);
	}

	float MaxHeight = 0.0f;
	for (const TPair<ECytraTileType, float>& Entry : TileHeights)
	{
		MaxHeight = FMath::Max(MaxHeight, Entry.Value);
	}

	// North-west corner of the first cell, south-east corner of the last one.
	const FVector NorthWest(CellSize * 0.5f, -CellSize * 0.5f, MaxHeight);
	FBox Local(ForceInit);
	Local += CellToLocal(FIntPoint(0, 0)) + NorthWest;
	Local += CellToLocal(FIntPoint(Map.Cols - 1, Map.Rows - 1)) - FVector(NorthWest.X, NorthWest.Y, 0.0f);
	return Local.TransformBy(GetActorTransform());
}
