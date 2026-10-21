// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Rules/CytraCrew.h"
#include "Rules/CytraGrid.h"
#include "World/CytraBoard.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UInstancedStaticMeshComponent* FindComponent(ACytraBoard& Board, const FName Name)
	{
		for (UInstancedStaticMeshComponent* Component : TInlineComponentArray<UInstancedStaticMeshComponent*>(&Board))
		{
			if (Component->GetFName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
	}

	UInstancedStaticMeshComponent* FindTiles(ACytraBoard& Board, const TCHAR* TypeName)
	{
		return FindComponent(Board, FName(FString::Printf(TEXT("Tiles_%s"), TypeName)));
	}

	int32 CountInstances(ACytraBoard& Board, const TCHAR* Name)
	{
		const UInstancedStaticMeshComponent* Component = FindComponent(Board, Name);
		return Component ? Component->GetInstanceCount() : 0;
	}

	bool GetTileColor(UInstancedStaticMeshComponent* Component, FLinearColor& OutColor)
	{
		const UMaterialInterface* Material = Component ? Component->GetMaterial(0) : nullptr;
		return Material && Material->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("Color")), OutColor);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraBoardBuildsPasse01Test, "Cytra.World.Board.BuildsPasse01",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCytraBoardBuildsPasse01Test::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);

	ACytraBoard* Board = World->SpawnActor<ACytraBoard>();
	if (TestNotNull(TEXT("Board spawned"), Board) && TestTrue(TEXT("Board built"), Board->EnsureBuilt()))
	{
		int32 Instances = 0;
		for (UInstancedStaticMeshComponent* Component : TInlineComponentArray<UInstancedStaticMeshComponent*>(Board))
		{
			Instances += Component->GetName().StartsWith(TEXT("Tiles_")) ? Component->GetInstanceCount() : 0;
		}
		TestEqual(TEXT("One instance per cell"), Instances, 24 * 14);
		TestEqual(TEXT("Rock instances"), FindTiles(*Board, TEXT("Rock")) ? FindTiles(*Board, TEXT("Rock"))->GetInstanceCount() : 0, Board->GetMap().Count(ECytraTileType::Rock));

		// GDD convention: grid x along +Y, grid y along -X, 200 units per cell.
		TestEqual(TEXT("Nest position"), Board->CellToWorld(FIntPoint(22, 7)), FVector(-1400.0, 4400.0, 0.0));
		const FBox Bounds = Board->GetBoardBounds();
		TestEqual(TEXT("Bounds min X"), Bounds.Min.X, -2700.0);
		TestEqual(TEXT("Bounds max X"), Bounds.Max.X, 100.0);
		TestEqual(TEXT("Bounds min Y"), Bounds.Min.Y, -100.0);
		TestEqual(TEXT("Bounds max Y"), Bounds.Max.Y, 4700.0);

		FLinearColor Rock, Road;
		TestTrue(TEXT("Rock material has a Color parameter"), GetTileColor(FindTiles(*Board, TEXT("Rock")), Rock));
		TestTrue(TEXT("Road material has a Color parameter"), GetTileColor(FindTiles(*Board, TEXT("Road")), Road));
		TestNotEqual(TEXT("Rock and road colors differ"), Rock, Road);

		// Picking.
		const FIntPoint Slot(13, 7);
		TestEqual(TEXT("World to cell"), Board->WorldToCell(Board->CellToWorld(Slot) + FVector(90.0, -90.0, 0.0)), TOptional<FIntPoint>(Slot));
		TestFalse(TEXT("Outside the board"), Board->WorldToCell(Board->CellToWorld(FIntPoint(-1, 3))).IsSet());
		TestEqual(TEXT("Straight down"), Board->CellUnderRay(Board->CellToWorld(Slot) + FVector(0.0, 0.0, 5000.0), FVector(0.0, 0.0, -1.0)), TOptional<FIntPoint>(Slot));
		// A slanted ray aimed at the top of the slot, near its east edge, crosses ground level in the next cell.
		const FVector Target = Board->CellToWorld(Slot) + FVector(0.0, 80.0, Board->GetTileHeight(ECytraTileType::Slot));
		const FVector Direction = FVector(0.0, 2.0, -1.0).GetSafeNormal();
		TestEqual(TEXT("Slanted ray"), Board->CellUnderRay(Target - Direction * 3000.0, Direction), TOptional<FIntPoint>(Slot));

		// Slot rings follow the reachable area; opened accesses change look.
		FCytraGrid Grid(Board->GetMap());
		const TArray<FCytraJob> NoJobs;
		Board->SyncGrid(Grid, NoJobs);
		TestEqual(TEXT("Reachable rings at start"), CountInstances(*Board, TEXT("SlotRing_Reachable")), 5);
		TestEqual(TEXT("Cut-off rings at start"), CountInstances(*Board, TEXT("SlotRing_CutOff")), 8);

		Grid.Open(FIntPoint(9, 6));
		Grid.Open(FIntPoint(13, 8));
		Board->SyncGrid(Grid, NoJobs);
		Board->SyncGrid(Grid, NoJobs);
		TestEqual(TEXT("Reachable rings after bridge and clearing"), CountInstances(*Board, TEXT("SlotRing_Reachable")), 8);
		TestEqual(TEXT("Cut-off rings after bridge and clearing"), CountInstances(*Board, TEXT("SlotRing_CutOff")), 5);
		TestEqual(TEXT("One bridge, drawn once"), CountInstances(*Board, TEXT("Bridge")), 1);
		TestEqual(TEXT("One cleared tile"), CountInstances(*Board, TEXT("Opened_Rubble")), 1);

		FCytraJob Job;
		Job.Cell = FIntPoint(13, 7);
		Board->SyncGrid(Grid, { Job });
		TestEqual(TEXT("No ring on a slot with a job"), CountInstances(*Board, TEXT("SlotRing_Reachable")), 7);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
