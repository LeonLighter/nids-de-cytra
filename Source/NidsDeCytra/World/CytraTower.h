// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/CytraLooks.h"
#include "World/CytraPiece.h"
#include "CytraTower.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Shows one tower on its slot: a squat base, a top piece that may turn toward the target, and one pip per level.
 * While it is being built, it stands at a fraction of its size.
 */
UCLASS()
class NIDSDECYTRA_API ACytraTower : public ACytraPiece
{
	GENERATED_BODY()

public:
	ACytraTower();

	void SetTowerLook(FName InType, const FCytraTowerLook& Look, float CellSize);
	FName GetType() const { return Type; }

	/**
	 * Level 0 while being built: no pips. AimYaw is the world yaw of the target, in degrees, used by aiming towers.
	 * Scale applies to the whole tower.
	 */
	void ShowState(int32 Level, float AimYaw, float Scale);

private:
	/** Turns with the aim and carries the top. */
	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<USceneComponent> Turret;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UStaticMeshComponent> Top;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UInstancedStaticMeshComponent> Pips;

	FName Type;
	bool bAims = false;
	float CellSize = 0.0f;
	int32 ShownLevel = -1;
	float ShownScale = -1.0f;
};
