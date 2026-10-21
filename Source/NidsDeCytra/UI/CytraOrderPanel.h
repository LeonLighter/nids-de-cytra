// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/SlateWrapperTypes.h"
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

private:
	UFUNCTION()
	void HandleClicked();

	TFunction<void()> Callback;
};

/**
 * Order panel on the right of the screen: what the selected cell is and the orders it takes, with their cost,
 * specialty, duration and, when refused, the reason. Built in C++, no widget blueprint.
 * It redraws when the selection or the match changes, and only if its content differs.
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

	void AddTitle(const FCytraPanelView& View);
	void AddLine(const FCytraPanelLine& Line, int32 StepNumber);
	void AddAction(const FCytraPanelAction& Action);
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold, const FText& Text);
	UCytraPanelButton* MakeButton(const FLinearColor& Background, TFunction<void()> Callback);

	/** Keeps clicks on the panel from selecting the cell under it. */
	UFUNCTION()
	FEventReply HandleBackgroundMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	FString ShownSignature;
	bool bRefreshQueued = false;
	FDelegateHandle MatchChangedHandle;
	FDelegateHandle SelectionChangedHandle;
};
