// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/CytraPiece.h"
#include "CytraEnemy.generated.h"

/** Shows one enemy of the match, with a health bar once wounded and a halo while slowed. */
UCLASS()
class NIDSDECYTRA_API ACytraEnemy : public ACytraPiece
{
	GENERATED_BODY()

public:
	ACytraEnemy();

	virtual void SetLook(const FCytraLook& Look, float CellSize) override;

	/** Health between 0 and 1: the bar shows below 1. */
	void ShowStatus(float Health, bool bSlowed);

private:
	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> BarBack;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> BarFill;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> SlowHalo;

	/** World size of the bar, set with the look. */
	float BarLength = 0.0f;
	float BarWidth = 0.0f;
	float ShownHealth = -1.0f;
};
