// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/SlateWrapperTypes.h"
#include "Engine/TimerHandle.h"
#include "UI/CytraPanelView.h"
#include "CytraOrderPanel.generated.h"

class UTextBlock;
class UVerticalBox;

/** A button that runs its own callback: UMG click events carry no parameter. */
UCLASS()
class NIDSDECYTRA_API UCytraPanelButton : public UButton
{
	GENERATED_BODY()

public:
	/** Binds the click event; Callback runs on each click. */
	void SetCallback(TFunction<void()> InCallback);

	/** Binds the hover events; Callback runs with true when the cursor enters, false when it leaves. */
	void SetHoverCallback(TFunction<void(bool)> InCallback);

private:
	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	TFunction<void()> Callback;
	TFunction<void(bool)> HoverCallback;
};

/**
 * Column on the right of the screen, in two cards. The order card shows what the selected cell is and the orders it takes,
 * with their cost, specialty, duration and, when refused, the reason. The crew card below has the recruitment buttons.
 * Built in C++, no widget blueprint. Each card redraws when the selection or the match changes, and only if its content differs.
 */
UCLASS()
class NIDSDECYTRA_API UCytraOrderPanel : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Redraws on the next frame: a click handler must not destroy the button it runs from. */
	void QueueRefresh();
	void Refresh();

	/** A card with its background in Column. Returns the box its content goes in. */
	UVerticalBox* AddCard(UVerticalBox* Column, const TCHAR* Name);
	/** Replaces the content of a card. */
	void ShowCard(UVerticalBox* Box, const FCytraPanelView& View);
	void AddTitle(UVerticalBox* Box, const FCytraPanelView& View);
	void AddLine(UVerticalBox* Box, const FCytraPanelLine& Line, int32 StepNumber);
	void AddAction(UVerticalBox* Box, const FCytraPanelAction& Action);
	/** A worker of the crew list: a dot of its color, its name, its state and its dismiss button. */
	void AddWorker(UVerticalBox* Box, const FCytraPanelWorker& Worker, bool bLast);
	/** A first click on a dismiss button asks for a confirmation, a second one within 3 seconds dismisses. */
	void HandleDismissClicked(int32 WorkerId);
	/** Puts back the label of the dismiss button waiting for its confirmation, if any. */
	void DisarmDismiss();
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold, const FText& Text);
	UCytraPanelButton* MakeButton(const FLinearColor& Background, TFunction<void()> Callback);

	/** Keeps clicks on the panel from selecting the cell under it. */
	UFUNCTION()
	FEventReply HandleBackgroundMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	/** Content of the order card. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	/** Crew card: the title and the worker list, then the recruitment buttons, redrawn apart so that state changes leave the buttons alone. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> CrewList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> CrewButtons;

	/** State of each worker of the crew list, in its order: updated in place, so that the dismiss buttons stay under the cursor. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> WorkerStates;

	/** Dismiss button labels, by worker id. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTextBlock>> DismissLabels;

	TMap<int32, FText> DismissTexts;
	/** Worker whose dismiss button waits for its confirmation, 0 if none. */
	int32 DismissArmedWorker = 0;
	FTimerHandle DismissTimer;

	FString ShownSignature;
	FString ShownCrewListSignature;
	FString ShownCrewButtonsSignature;
	bool bRefreshQueued = false;
	FDelegateHandle MatchChangedHandle;
	FDelegateHandle SelectionChangedHandle;
};
