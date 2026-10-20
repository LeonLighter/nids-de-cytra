// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "World/CytraBoard.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UInstancedStaticMeshComponent* FindTiles(ACytraBoard& Board, const TCHAR* TypeName)
	{
		const FName Name(FString::Printf(TEXT("Tiles_%s"), TypeName));
		for (UInstancedStaticMeshComponent* Component : TInlineComponentArray<UInstancedStaticMeshComponent*>(&Board))
		{
			if (Component->GetFName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
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
			Instances += Component->GetInstanceCount();
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
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
