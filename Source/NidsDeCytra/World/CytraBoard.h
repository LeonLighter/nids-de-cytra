// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CytraMapDef.h"
#include "CytraBoard.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Draws the map grid with engine basic shapes, one instanced mesh per tile type.
 * Grid x runs along world +Y and grid y along world -X, so a top-down view reads like the map file.
 */
UCLASS()
class NIDSDECYTRA_API ACytraBoard : public AActor
{
	GENERATED_BODY()

public:
	static constexpr float CellSize = 200.0f;

	ACytraBoard();

	/** Loads the map file and creates the tiles, once. Returns false if the map could not be loaded. */
	bool EnsureBuilt();

	const FCytraMapDef& GetMap() const { return Map; }

	/** World position of the centre of a cell, at ground level. */
	FVector CellToWorld(const FIntPoint& Cell) const;

	/** World-space box covering every tile. Invalid if the board is not built. */
	FBox GetBoardBounds() const;

protected:
	virtual void BeginPlay() override;

	/** Map data file, relative to the Content folder. */
	UPROPERTY(EditAnywhere, Category = "Cytra")
	FString MapFile = TEXT("Data/Maps/Passe01.json");

	UPROPERTY(EditAnywhere, Category = "Cytra")
	TMap<ECytraTileType, FLinearColor> TileColors;

	/** Height of each tile type, in world units. */
	UPROPERTY(EditAnywhere, Category = "Cytra")
	TMap<ECytraTileType, float> TileHeights;

	/** Space left between neighbouring tiles, in world units. */
	UPROPERTY(EditAnywhere, Category = "Cytra", meta = (ClampMin = "0.0"))
	float TileGap = 12.0f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> TileMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY(Transient)
	TMap<ECytraTileType, TObjectPtr<UInstancedStaticMeshComponent>> TileComponents;

private:
	UInstancedStaticMeshComponent* GetOrCreateTileComponent(ECytraTileType Type);
	float GetTileHeight(ECytraTileType Type) const;

	FCytraMapDef Map;
	bool bBuilt = false;
};
