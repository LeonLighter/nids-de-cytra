// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraBoard.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "CytraLog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Rules/CytraCrew.h"
#include "Rules/CytraGrid.h"
#include "Rules/CytraMapLoader.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraShapes.h"

namespace
{
	const FName ColorParameter(TEXT("Color"));

	/** A grid position in board space: grid x along +Y, grid y along -X. */
	FVector GridToLocal(const FVector2d& Position)
	{
		return FVector(-Position.Y * ACytraBoard::CellSize, Position.X * ACytraBoard::CellSize, 0.0f);
	}

	/** Centre of a cell in board space. */
	FVector CellToLocal(const FIntPoint& Cell)
	{
		return GridToLocal(FVector2d(Cell.X, Cell.Y));
	}

	// Opened accesses and slot marks, after the web prototype (sizes converted from its 40-pixel cells).
	const TCHAR* const BridgeColor = TEXT("a67a45");
	const TCHAR* const LadderColor = TEXT("d6a868");
	const TCHAR* const ReachableColor = TEXT("f0a04b");
	const TCHAR* const CutOffColor = TEXT("9ca8b8");
	constexpr float BridgeWidth = 22.0f / 40.0f;
	constexpr float LadderWidth = 14.0f / 40.0f;
	constexpr float RingOuter = 24.0f / 40.0f;
	constexpr float RingInner = 18.0f / 40.0f;
	/** Thickness of flat marks, in world units. */
	constexpr float MarkThickness = 3.0f;
}

ACytraBoard::ACytraBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	TileMesh = CubeFinder.Object;
	DiscMesh = CylinderFinder.Object;
	TileMaterial = MaterialFinder.Object;

	// Placeholder palette, after the web prototype.
	TileColors = {
		{ ECytraTileType::Rock, CytraShapes::FromHex(TEXT("2a3340")) },
		{ ECytraTileType::Road, CytraShapes::FromHex(TEXT("6d6351")) },
		{ ECytraTileType::Ledge, CytraShapes::FromHex(TEXT("9ca8b8")) },
		{ ECytraTileType::Slot, CytraShapes::FromHex(TEXT("505b6d")) },
		{ ECytraTileType::HighSlot, CytraShapes::FromHex(TEXT("667286")) },
		{ ECytraTileType::Rubble, CytraShapes::FromHex(TEXT("8a8072")) },
		{ ECytraTileType::Gap, CytraShapes::FromHex(TEXT("05070a")) },
		{ ECytraTileType::Cliff, CytraShapes::FromHex(TEXT("566173")) },
		{ ECytraTileType::Camp, CytraShapes::FromHex(TEXT("8cc48d")) },
		{ ECytraTileType::OutpostSite, CytraShapes::FromHex(TEXT("8a5a2c")) },
		{ ECytraTileType::Mine, CytraShapes::FromHex(TEXT("f2cf63")) },
		{ ECytraTileType::Nest, CytraShapes::FromHex(TEXT("bba8ea")) },
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

	const float Footprint = (CellSize - TileGap) / CytraShapes::Size;
	for (int32 Y = 0; Y < Map.Rows; ++Y)
	{
		for (int32 X = 0; X < Map.Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			const ECytraTileType Type = Map.Get(Cell);
			const float Height = GetTileHeight(Type);
			const FVector Center = CellToLocal(Cell) + FVector(0.0f, 0.0f, Height * 0.5f);
			const int32 Instance = GetOrCreateTileComponent(Type)->AddInstance(FTransform(FQuat::Identity, Center, FVector(Footprint, Footprint, Height / CytraShapes::Size)));
			if (IsCytraOpenable(Type))
			{
				OpenableInstances.Add(Cell, Instance);
			}
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
	const FLinearColor* Color = TileColors.Find(Type);
	if (!Color)
	{
		UE_LOG(LogCytra, Warning, TEXT("Board: no color for tile type %s"), *TypeName);
	}
	UInstancedStaticMeshComponent* Component = CreateInstances(*FString::Printf(TEXT("Tiles_%s"), *TypeName), TileMesh, Color ? *Color : FLinearColor::White);
	TileComponents.Add(Type, Component);
	return Component;
}

UInstancedStaticMeshComponent* ACytraBoard::GetOrCreateOverlay(FName Name, UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (const TObjectPtr<UInstancedStaticMeshComponent>* Existing = OverlayComponents.Find(Name))
	{
		return *Existing;
	}
	UInstancedStaticMeshComponent* Component = CreateInstances(Name, Mesh, Color);
	OverlayComponents.Add(Name, Component);
	return Component;
}

UInstancedStaticMeshComponent* ACytraBoard::CreateInstances(FName Name, UStaticMesh* Mesh, const FLinearColor& Color)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Name);
	Component->SetupAttachment(RootComponent);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TileMaterial, Component);
	Material->SetVectorParameterValue(ColorParameter, Color);
	Component->SetMaterial(0, Material);

	Component->RegisterComponent();
	return Component;
}

void ACytraBoard::SyncGrid(const FCytraGrid& Grid, const TArray<FCytraJob>& Jobs)
{
	if (!bBuilt)
	{
		return;
	}
	for (const TPair<FIntPoint, int32>& Openable : OpenableInstances)
	{
		if (Grid.IsOpened(Openable.Key) && !ShownOpened.Contains(Openable.Key))
		{
			ShownOpened.Add(Openable.Key);
			ShowOpened(Openable.Key);
		}
	}

	// A ring on every free slot: orange if workers can reach it, gray if it is cut off from the camp.
	UInstancedStaticMeshComponent* Reachable = GetOrCreateOverlay(TEXT("SlotRing_Reachable"), DiscMesh, CytraShapes::FromHex(ReachableColor));
	UInstancedStaticMeshComponent* CutOff = GetOrCreateOverlay(TEXT("SlotRing_CutOff"), DiscMesh, CytraShapes::FromHex(CutOffColor));
	UInstancedStaticMeshComponent* Inner = GetOrCreateOverlay(TEXT("SlotRing_Inner"), DiscMesh, TileColors.FindRef(ECytraTileType::Slot));
	UInstancedStaticMeshComponent* HighInner = GetOrCreateOverlay(TEXT("SlotRing_HighInner"), DiscMesh, TileColors.FindRef(ECytraTileType::HighSlot));
	Reachable->ClearInstances();
	CutOff->ClearInstances();
	Inner->ClearInstances();
	HighInner->ClearInstances();
	for (int32 Y = 0; Y < Map.Rows; ++Y)
	{
		for (int32 X = 0; X < Map.Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			const ECytraTileType Type = Map.Get(Cell);
			const bool bBusy = Grid.GetTower(Cell) || Jobs.ContainsByPredicate([&Cell](const FCytraJob& Job) { return Job.Cell == Cell; });
			if (!IsCytraSlot(Type) || bBusy)
			{
				continue;
			}
			const float Top = GetTileHeight(Type);
			(Grid.IsReachable(Cell) ? Reachable : CutOff)->AddInstance(CellBox(Cell, RingOuter, RingOuter, Top, MarkThickness));
			(Type == ECytraTileType::HighSlot ? HighInner : Inner)->AddInstance(CellBox(Cell, RingInner, RingInner, Top, MarkThickness + 1.0f));
		}
	}
}

void ACytraBoard::ShowOpened(const FIntPoint& Cell)
{
	const ECytraTileType Type = Map.Get(Cell);
	const auto Hide = [this, &Cell, Type]()
	{
		const FTransform Hidden(FQuat::Identity, CellToLocal(Cell) - FVector(0.0f, 0.0f, CellSize), FVector(KINDA_SMALL_NUMBER));
		TileComponents.FindChecked(Type)->UpdateInstanceTransform(OpenableInstances.FindChecked(Cell), Hidden, false, true);
	};
	const float Footprint = 1.0f - TileGap / CellSize;

	switch (Type)
	{
	case ECytraTileType::Rubble:
		// Cleared: plain floor, like the road.
		Hide();
		GetOrCreateOverlay(TEXT("Opened_Rubble"), TileMesh, TileColors.FindRef(ECytraTileType::Road))
			->AddInstance(CellBox(Cell, Footprint, Footprint, 0.0f, GetTileHeight(ECytraTileType::Road)));
		break;
	case ECytraTileType::Gap:
		// Planks from edge to edge, level with the road.
		GetOrCreateOverlay(TEXT("Bridge"), TileMesh, CytraShapes::FromHex(BridgeColor))
			->AddInstance(CellBox(Cell, 1.0f, BridgeWidth, 0.0f, GetTileHeight(ECytraTileType::Road)));
		break;
	case ECytraTileType::Cliff:
		// A ladder laid over the cliff face.
		GetOrCreateOverlay(TEXT("Ladder"), TileMesh, CytraShapes::FromHex(LadderColor))
			->AddInstance(CellBox(Cell, Footprint, LadderWidth, GetTileHeight(Type), MarkThickness));
		break;
	case ECytraTileType::OutpostSite:
		// Raised: it is a camp now.
		Hide();
		GetOrCreateOverlay(TEXT("Opened_OutpostSite"), TileMesh, TileColors.FindRef(ECytraTileType::Camp))
			->AddInstance(CellBox(Cell, Footprint, Footprint, 0.0f, GetTileHeight(Type)));
		break;
	default:
		// The open mine is shown in J50.
		break;
	}
}

FTransform ACytraBoard::CellBox(const FIntPoint& Cell, float Along, float Across, float Bottom, float Height) const
{
	// The passage runs along grid x when both east and west neighbours are open ground, otherwise along grid y.
	const bool bAlongX = Map.Get(Cell - FIntPoint(1, 0)) != ECytraTileType::Rock && Map.Get(Cell + FIntPoint(1, 0)) != ECytraTileType::Rock;
	const float SizeX = (bAlongX ? Along : Across) * CellSize / CytraShapes::Size;
	const float SizeY = (bAlongX ? Across : Along) * CellSize / CytraShapes::Size;
	// Board axes: grid x along +Y, grid y along -X.
	return FTransform(FQuat::Identity, CellToLocal(Cell) + FVector(0.0f, 0.0f, Bottom + Height * 0.5f), FVector(SizeY, SizeX, Height / CytraShapes::Size));
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

FVector ACytraBoard::GridToWorld(const FVector2d& Position) const
{
	return GetActorTransform().TransformPosition(GridToLocal(Position));
}

TOptional<FIntPoint> ACytraBoard::WorldToCell(const FVector& World) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(World);
	const FIntPoint Cell(FMath::RoundToInt32(Local.Y / CellSize), FMath::RoundToInt32(-Local.X / CellSize));
	return Map.IsInside(Cell) ? TOptional<FIntPoint>(Cell) : TOptional<FIntPoint>();
}

TOptional<FIntPoint> ACytraBoard::CellUnderRay(const FVector& Origin, const FVector& Direction) const
{
	const FVector Up = GetActorUpVector();
	const auto HitAtHeight = [&](float Height) -> TOptional<FVector>
	{
		const FPlane Plane(GetActorLocation() + Up * Height, Up);
		const double Denominator = FVector::DotProduct(Direction, Up);
		if (FMath::IsNearlyZero(Denominator))
		{
			return {};
		}
		return FMath::RayPlaneIntersection(Origin, Direction, Plane);
	};

	// Tiles have different heights: aim at ground level, then again at the top of the tile found there.
	TOptional<FVector> Hit = HitAtHeight(0.0f);
	TOptional<FIntPoint> Cell = Hit ? WorldToCell(*Hit) : TOptional<FIntPoint>();
	for (int32 Pass = 0; Pass < 2 && Cell; ++Pass)
	{
		Hit = HitAtHeight(GetTileHeight(Map.Get(*Cell)));
		const TOptional<FIntPoint> Next = Hit ? WorldToCell(*Hit) : TOptional<FIntPoint>();
		if (!Next || *Next == *Cell)
		{
			break;
		}
		Cell = Next;
	}
	return Cell;
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
