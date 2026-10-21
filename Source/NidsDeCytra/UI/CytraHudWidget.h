// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CytraHudWidget.generated.h"

class UButton;
class UHorizontalBox;
class UTextBlock;

/**
 * Top bar: gold, eggs, wave, what comes next and the launch button. Built in C++, no widget blueprint.
 * It refreshes when the game state reports a change, and relays clicks to the player controller.
 */
UCLASS()
class NIDSDECYTRA_API UCytraHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Adds a label over a value to Row and returns the value text. */
	UTextBlock* AddStat(UHorizontalBox* Row, const TCHAR* LabelKey, const FLinearColor& ValueColor);
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);

	void Refresh();

	UFUNCTION()
	void HandleLaunchClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EggsValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LaunchLabel;

	FDelegateHandle MatchChangedHandle;
};
