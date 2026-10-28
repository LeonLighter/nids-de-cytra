// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "CytraHudWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCytraPanelButton;
class UHorizontalBox;
class UOverlay;
class UTextBlock;
class UVerticalBox;
struct FCytraMessage;

/**
 * The whole screen of the game, built in C++, no widget blueprint:
 * - the top bar: gold, eggs, wave, what comes next with its threat, the launch button and the time controls;
 * - the order panel on the right;
 * - over the board: the brief messages, the end screen, and the names and gold written on the map.
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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildBar(UVerticalBox* Layout);
	void BuildBoardArea(UOverlay* Area);

	/** Adds a label over a value to Row and returns the value text. */
	UTextBlock* AddStat(UHorizontalBox* Row, const TCHAR* LabelKey, const FLinearColor& ValueColor);
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
	/** A button of the time controls, with its label. */
	UCytraPanelButton* AddControl(UHorizontalBox* Row, TObjectPtr<UTextBlock>& OutLabel, TFunction<void()> Callback);

	void Refresh();
	void ShowEnd();
	void ShowMessage(const FCytraMessage& Message);

	/** Places the map names and the floating gold over the board, where the camera shows them. */
	void PlaceMapText();

	UFUNCTION()
	void HandleLaunchClicked();

	/** Keeps clicks on the end screen from selecting the cell under it. */
	UFUNCTION()
	FEventReply HandleEndMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EggsValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NextText;

	/** Threat label and gauge, and the forecast of the next wave: shown during preparation. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ThreatBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ThreatCells;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ForecastText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LaunchLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PauseLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RestartLabel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ToastFrame;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ToastBack;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToastText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndScreen;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndBody;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndProgress;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndStats;

	/** Names and gold written on the map. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MapText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> MapLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> GoldTexts;

	/** Real time, in seconds, when the message hides and when the restart button stops waiting for its confirmation. */
	double ToastHideTime = 0.0;
	double RestartArmedUntil = 0.0;
	/** Screen size of a cell the map texts were last sized for. */
	float MapTextCellSize = 0.0f;

	FDelegateHandle MatchChangedHandle;
	FDelegateHandle MessageHandle;
};
