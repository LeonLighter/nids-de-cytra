// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraMarkers.generated.h"

class ACytraBoard;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
struct FCytraJob;

/**
 * Marks drawn over the board: the selected and hovered cells, a progress bar on every job, the range of a slot
 * and the arrows of the next wave. It does not tick: the game mode updates it.
 */
UCLASS()
class NIDSDECYTRA_API ACytraMarkers : public AActor
{
	GENERATED_BODY()

public:
	ACytraMarkers();

	/** Draws a frame around Cell on Board, or hides it. */
	void ShowSelection(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell);

	/** Draws a thin light frame around the cell under the cursor, or hides it. */
	void ShowHover(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell);

	/**
	 * Two red arrows at the start of each of these routes (indices into the map routes), for the next wave.
	 * Pulse, from 0 to 1, moves them forward and brightens them.
	 */
	void ShowNextRoutes(const ACytraBoard& Board, const TArray<int32>& Routes, float Pulse);

	/** One bar per job: orange once a worker is assigned, gray with three dots while it waits for one. */
	void ShowJobs(const ACytraBoard& Board, const TArray<FCytraJob>& Jobs);

	/**
	 * Draws a circle of Radius cells around Cell: the range of the tower there, dashed for a tower not built yet.
	 * Hidden when Cell is unset.
	 */
	void ShowRange(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell, double Radius, bool bDashed);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SelectionFrame;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> HoverFrame;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> RangeRing;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> RouteArrows;

	/** Color of the arrows, which blink. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RouteArrowMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> BarBacks;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> BarFills;

	/** Fills and dots of jobs still waiting for a worker. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> WaitingMarks;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	/** A flat-colored instanced cube component. */
	UInstancedStaticMeshComponent* MakeBoxes(FName Name, const TCHAR* Hex);

	/** Four boxes around Cell, Inset inside its edges and Width wide, in cells of the prototype. */
	static void AddFrame(UInstancedStaticMeshComponent& Boxes, const ACytraBoard& Board, const FIntPoint& Cell, float Inset, float Width);
};
