// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CytraPlayerController.generated.h"

class UCytraHudWidget;
class UInputAction;
class UInputMappingContext;
struct FCytraOrderAction;

DECLARE_MULTICAST_DELEGATE(FOnCytraSelectionChanged);

/** Mouse-driven controller: the cursor stays visible, the game is played by clicking tiles. */
UCLASS()
class NIDSDECYTRA_API ACytraPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACytraPlayerController();

	/** Order from the interface: launch the next wave now. */
	void LaunchWave();

	/** Order from the interface on the selected cell. */
	void OrderOnSelection(const FCytraOrderAction& Action);

	/** Order from the interface: recruit a worker of this specialty. */
	void Recruit(FName Specialty);

	/** The order panel points at a tower type, or at none: an empty selected slot shows its range. */
	void PreviewTower(FName Tower);

	/** Selects a cell of the board, or clears the selection. */
	void SelectCell(const TOptional<FIntPoint>& Cell);
	const TOptional<FIntPoint>& GetSelectedCell() const { return SelectedCell; }

	/** Broadcast when the selected cell changes. */
	FOnCytraSelectionChanged OnSelectionChanged;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(Transient)
	TObjectPtr<UCytraHudWidget> Hud;

	/** Input objects are created in code: the game has no input assets. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ClickAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputContext;

private:
	void CreateInputObjects();
	/** Selects the cell under the cursor. Clicks outside the board change nothing. */
	void HandleClick();

	TOptional<FIntPoint> SelectedCell;
};
