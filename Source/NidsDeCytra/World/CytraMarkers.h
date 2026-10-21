// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraMarkers.generated.h"

class ACytraBoard;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** Marks drawn over the board: the selected cell. It does not tick: the game mode updates it. */
UCLASS()
class NIDSDECYTRA_API ACytraMarkers : public AActor
{
	GENERATED_BODY()

public:
	ACytraMarkers();

	/** Draws a frame around Cell on Board, or hides it. */
	void ShowSelection(const ACytraBoard& Board, const TOptional<FIntPoint>& Cell);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SelectionFrame;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	/** A flat-colored instanced cube component. */
	UInstancedStaticMeshComponent* MakeBoxes(FName Name, const TCHAR* Hex);
};
