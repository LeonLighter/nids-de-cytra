// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/CytraIsoView.h"
#include "GameFramework/Pawn.h"
#include "CytraCameraPawn.generated.h"

class UCameraComponent;

/**
 * Orthographic isometric camera, seen from the south-west by default: the west entries on the left, the nest on the right.
 * It shows the whole board beside the interface, zooms with the mouse wheel and pans once zoomed in.
 */
UCLASS()
class NIDSDECYTRA_API ACytraCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ACytraCameraPawn();

	/** Angle, zoom range and panning speed. Takes effect at the next FrameBounds. */
	void SetCameraLook(const FCytraCameraLook& InLook);

	/** Shows the whole box beside the interface. The view is kept over the box when the viewport is resized. */
	void FrameBounds(const FBox& Bounds);

	/** Moves the view along the screen, X to the right and Y up, each between -1 and 1, for DeltaSeconds of real time. */
	void Pan(const FVector2D& Direction, float DeltaSeconds);

	/** Zooms by mouse wheel notches, positive toward the board, keeping the point under the cursor in place. */
	void ZoomAt(float Notches, const FVector2D& CursorPixels);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Cytra")
	TObjectPtr<UCameraComponent> Camera;

private:
	void HandleViewportResized(FViewport* Viewport, uint32 Unused);

	/** The viewport in pixels and the space the interface takes on it. */
	FCytraScreen GetScreen() const;
	void ApplyView();

	FCytraCameraLook Look;
	FCytraIsoView View;
	bool bFramed = false;
	FDelegateHandle ResizedHandle;
};
