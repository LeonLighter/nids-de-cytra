// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CytraNest.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** Cytra's nest: one small egg per egg left, laid out in a spiral as in the web prototype. */
UCLASS()
class NIDSDECYTRA_API ACytraNest : public AActor
{
	GENERATED_BODY()

public:
	ACytraNest();

	/** Shows Count eggs around the actor location. */
	void SetEggs(int32 Count, float CellSize);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UInstancedStaticMeshComponent> Eggs;

	UPROPERTY(EditAnywhere, Category = "Cytra")
	FLinearColor EggColor;

	UPROPERTY()
	TObjectPtr<UStaticMesh> EggMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> EggMaterial;

private:
	int32 ShownEggs = INDEX_NONE;
};
