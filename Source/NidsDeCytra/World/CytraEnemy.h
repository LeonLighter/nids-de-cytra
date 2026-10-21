// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraEnemy.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FCytraLook;

/** Shows one enemy of the match. It does not tick: the game mode places it after each frame of simulation. */
UCLASS()
class NIDSDECYTRA_API ACytraEnemy : public AActor
{
	GENERATED_BODY()

public:
	ACytraEnemy();

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
