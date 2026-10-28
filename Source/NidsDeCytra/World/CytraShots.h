// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rules/CytraMatch.h"
#include "CytraShots.generated.h"

class ACytraBoard;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * Projectiles in flight and brief effects (coil zaps, shell blasts), drawn as instanced basic shapes:
 * one actor for all of them, since a wave can have dozens of bolts in the air.
 * It does not tick: the game mode updates it after each frame of simulation.
 */
UCLASS()
class NIDSDECYTRA_API ACytraShots : public AActor
{
	GENERATED_BODY()

public:
	ACytraShots();

	/** Redraws the shots in flight, starts the new effects and ages the running ones by DeltaSeconds. */
	void Show(const ACytraBoard& Board, const TArray<FCytraShot>& Shots, TArray<FCytraEffect> NewEffects, float DeltaSeconds);

private:
	struct FRunningEffect
	{
		FCytraEffect Effect;
		float Age = 0.0f;
	};

	UInstancedStaticMeshComponent* MakeInstances(FName Name, UStaticMesh* Mesh, const TCHAR* Hex);

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Bolts;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Shells;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Zaps;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Blasts;

	TArray<FRunningEffect> Running;
};
