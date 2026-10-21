// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraPiece.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FCytraLook;

/**
 * A moving piece of the match shown as an engine basic shape: an enemy or a worker.
 * It does not tick: the game mode places it after each frame of simulation.
 */
UCLASS(Abstract)
class NIDSDECYTRA_API ACytraPiece : public AActor
{
	GENERATED_BODY()

public:
	ACytraPiece();

	/** Sets shape, color and size. The shape rests on the actor location. */
	void SetLook(const FCytraLook& Look, float CellSize);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Engine basic shapes by name, loaded in the constructor so they are cooked with the game. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> ShapeMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;
};
