// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CytraMapDef.h"
#include "CytraBoard.generated.h"

class FCytraGrid;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
struct FCytraJob;

/**
 * Draws the map grid with engine basic shapes, one instanced mesh per tile type.
 * Grid x runs along world +Y and grid y along world -X: seen from above, the board reads like the map file.
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

	/** World position of a fractional grid position, at ground level. Cell centres are at whole coordinates. */
	FVector GridToWorld(const FVector2d& Position) const;

	/** The cell under a world position, ignoring height. Unset outside the board. */
	TOptional<FIntPoint> WorldToCell(const FVector& World) const;

	/** The tile block a ray hits first, on its top or its side, for mouse picking. Unset outside the board. */
	TOptional<FIntPoint> CellUnderRay(const FVector& Origin, const FVector& Direction) const;

	/** Height of the top of a tile type, in world units above ground level. */
	float GetTileHeight(ECytraTileType Type) const;

	/** World-space box covering every tile. Invalid if the board is not built. */
	FBox GetBoardBounds() const;

	/**
	 * Shows the accesses opened so far, and marks each free slot as reachable or cut off from the camp.
	 * Call it whenever the grid or the jobs may have changed.
	 */
	void SyncGrid(const FCytraGrid& Grid, const TArray<FCytraJob>& Jobs);

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
	TObjectPtr<UStaticMesh> DiscMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY(Transient)
	TMap<ECytraTileType, TObjectPtr<UInstancedStaticMeshComponent>> TileComponents;

	/** Bridges, ladders, cleared tiles and slot marks, by name. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> OverlayComponents;

private:
	UInstancedStaticMeshComponent* GetOrCreateTileComponent(ECytraTileType Type);
	UInstancedStaticMeshComponent* GetOrCreateOverlay(FName Name, UStaticMesh* Mesh, const FLinearColor& Color);
	UInstancedStaticMeshComponent* CreateInstances(FName Name, UStaticMesh* Mesh, const FLinearColor& Color);

	/** Changes the look of a cell once a job has opened it. */
	void ShowOpened(const FIntPoint& Cell);
	/** Instance transform of a box on a cell: Along cells long in the passage direction, Across cells wide. */
	FTransform CellBox(const FIntPoint& Cell, float Along, float Across, float Bottom, float Height) const;

	FCytraMapDef Map;
	bool bBuilt = false;
	/** Tile instance of every openable cell, to hide it once opened. */
	TMap<FIntPoint, int32> OpenableInstances;
	TSet<FIntPoint> ShownOpened;
};
