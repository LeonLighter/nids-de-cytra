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
 * A piece of the match shown as engine basic shapes: an enemy, a worker or a tower.
 * It does not tick: the game mode places it after each frame of simulation.
 */
UCLASS(Abstract)
class NIDSDECYTRA_API ACytraPiece : public AActor
{
	GENERATED_BODY()

public:
	ACytraPiece();

	/** Sets shape, color and size. The shape rests on the actor location. */
	virtual void SetLook(const FCytraLook& Look, float CellSize);

protected:
	/** Gives Component the shape and a flat color of Look. False if the shape or the material is missing. */
	bool ApplyLook(UStaticMeshComponent& Component, const FCytraLook& Look);

	/** A flat-colored copy of the base material, owned by this actor. */
	UMaterialInterface* MakeColor(const FLinearColor& Color);

	/** A component of this actor without collision, attached to Parent. Constructor only. */
	UStaticMeshComponent* CreatePart(FName Name, USceneComponent* Parent);

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Engine basic shapes by name, loaded in the constructor so they are cooked with the game. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UStaticMesh>> ShapeMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;
};
