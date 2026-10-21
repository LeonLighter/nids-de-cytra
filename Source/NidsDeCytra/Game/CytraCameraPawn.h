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

	/**
	 * Moves the camera straight above the box, high enough for all of it to fit on screen beside the interface.
	 * The framing is redone when the viewport is resized.
	 */
	void FrameBounds(const FBox& Bounds);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UCameraComponent> Camera;

	/** Extra space around the framed box, as a factor of its size. */
	UPROPERTY(EditAnywhere, Category = "Cytra", meta = (ClampMin = "1.0"))
	float FramingMargin = 1.08f;

private:
	void HandleViewportResized(FViewport* Viewport, uint32 Unused);

	FBox FramedBounds = FBox(ForceInit);
	FDelegateHandle ResizedHandle;
};
