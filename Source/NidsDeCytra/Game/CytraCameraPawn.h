// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CytraCameraPawn.generated.h"

class UCameraComponent;

/** Fixed top-down camera. Screen right is world +Y (grid x), screen down is world -X (grid y). */
UCLASS()
class NIDSDECYTRA_API ACytraCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ACytraCameraPawn();

	/** Moves the camera straight above the box, high enough for all of it to fit on screen. */
	void FrameBounds(const FBox& Bounds);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UCameraComponent> Camera;

	/** Extra space around the framed box, as a factor of its size. */
	UPROPERTY(EditAnywhere, Category = "Cytra", meta = (ClampMin = "1.0"))
	float FramingMargin = 1.08f;
};
