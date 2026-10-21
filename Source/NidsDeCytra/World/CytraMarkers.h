// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraMarkers.generated.h"

class ACytraBoard;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
struct FCytraJob;

/** Marks drawn over the board: the selected cell and a progress bar on every job. It does not tick: the game mode updates it. */
UCLASS()
class NIDSDECYTRA_API ACytraMarkers : public AActor
{
	GENERATED_BODY()

public:
	ACytraMarkers();

	/** Draws a frame around Cell on Board, or hides it. */
	void ShowSelection(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell);

	/** One bar per job: orange once a worker is assigned, gray with three dots while it waits for one. */
	void ShowJobs(const ACytraBoard& Board, const TArray<FCytraJob>& Jobs);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SelectionFrame;

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
};
