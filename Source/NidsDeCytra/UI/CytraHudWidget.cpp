// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraHudWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Game/CytraGameMode.h"
#include "Game/CytraGameState.h"
#include "Game/CytraPlayerController.h"
#include "HAL/PlatformTime.h"
#include "UI/CytraHudView.h"
#include "UI/CytraLayout.h"
#include "UI/CytraMessages.h"
#include "UI/CytraOrderPanel.h"
#include "UI/CytraPalette.h"
#include "UI/CytraText.h"
#include "World/CytraBoard.h"

namespace
{
	/** How long a message stays, and how long the restart button waits for its confirmation, in real seconds. */
	constexpr double ToastSeconds = 2.6;
	constexpr double RestartConfirmSeconds = 3.0;

	/** Gold text of the prototype: 13 pixels, starting 10 pixels above the point and rising 16 more. */
	constexpr float GoldTextSize = 13.0f;
	constexpr float GoldRise = 10.0f / 40.0f;
	constexpr float GoldRiseMore = 16.0f / 40.0f;

	/** Slate font sizes are in points: 3 pixels for 4 points. */
	constexpr float PointsPerPixel = 0.75f;

	const FCytraPalette& Palette()
	{
		return FCytraPalette::Get();
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A = Alpha;
		return Color;
	}
}

void UCytraHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The HUD covers the whole screen: only the bar, the panel and the end screen may catch clicks, the rest goes through to the board.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// Under everything else, the texts written on the map, placed in screen space.
	MapText = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapText"));
	MapText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* MapTextSlot = Root->AddChildToOverlay(MapText);
	MapTextSlot->SetHorizontalAlignment(HAlign_Fill);
	MapTextSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Layout"));
	Layout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* LayoutSlot = Root->AddChildToOverlay(Layout);
	LayoutSlot->SetHorizontalAlignment(HAlign_Fill);
	LayoutSlot->SetVerticalAlignment(VAlign_Fill);

	BuildBar(Layout);

	// Under the bar: the board area on the left, the order panel on the right; the camera keeps the board clear of both.
	UHorizontalBox* Below = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Below"));
	Below->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBoxSlot* BelowSlot = Layout->AddChildToVerticalBox(Below);
	BelowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UOverlay* BoardArea = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BoardArea"));
	BoardArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UHorizontalBoxSlot* AreaSlot = Below->AddChildToHorizontalBox(BoardArea);
	AreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AreaSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 12.0f));
	BuildBoardArea(BoardArea);

	constexpr float PanelMargin = 12.0f;
	USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelBox"));
	PanelBox->SetWidthOverride(CytraLayout::PanelWidth - 2.0f * PanelMargin);
	PanelBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCytraOrderPanel* Panel = CreateWidget<UCytraOrderPanel>(GetOwningPlayer(), UCytraOrderPanel::StaticClass()))
	{
		PanelBox->SetContent(Panel);
	}
	UHorizontalBoxSlot* PanelSlot = Below->AddChildToHorizontalBox(PanelBox);
	PanelSlot->SetPadding(FMargin(PanelMargin, 0.0f, PanelMargin, PanelMargin));
	PanelSlot->SetVerticalAlignment(VAlign_Top);
}

void UCytraHudWidget::BuildBar(UVerticalBox* Layout)
{
	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bar"));
	Bar->SetBrushColor(Palette().Rock900);
	Bar->SetPadding(FMargin(14.0f, 10.0f));
	UVerticalBoxSlot* BarSlot = Layout->AddChildToVerticalBox(Bar);
	BarSlot->SetPadding(FMargin(12.0f));
	BarSlot->SetHorizontalAlignment(HAlign_Fill);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Bar->SetContent(Row);

	GoldValue = AddStat(Row, TEXT("Hud.Gold"), Palette().Gold);
	EggsValue = AddStat(Row, TEXT("Hud.Eggs"), Palette().Silk);
	WaveValue = AddStat(Row, TEXT("Hud.Wave"), Palette().Snow);

	// What comes next: "Vague 2 dans 24 s.", then during preparation the threat in five cells and the forecast.
	UHorizontalBox* Next = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Next"));
	UHorizontalBoxSlot* NextSlot = Row->AddChildToHorizontalBox(Next);
	NextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NextSlot->SetVerticalAlignment(VAlign_Center);
	NextSlot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));

	NextText = MakeText(15, Palette().Snow, false);
	Next->AddChildToHorizontalBox(NextText)->SetVerticalAlignment(VAlign_Center);

	ThreatBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Threat"));
	UHorizontalBoxSlot* ThreatSlot = Next->AddChildToHorizontalBox(ThreatBox);
	ThreatSlot->SetVerticalAlignment(VAlign_Center);
	ThreatSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	UTextBlock* ThreatLabel = MakeText(15, Palette().Mist, false);
	ThreatLabel->SetText(CytraText::Get(TEXT("Hud.Threat")));
	ThreatBox->AddChildToHorizontalBox(ThreatLabel)->SetVerticalAlignment(VAlign_Center);
	ThreatCells.Reset();
	for (int32 Index = 0; Index < 5; ++Index)
	{
		// A 7 x 12 pixel cell with a 1-pixel red edge, filled when on.
		USizeBox* CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CellBox->SetWidthOverride(7.0f);
		CellBox->SetHeightOverride(12.0f);
		UBorder* Edge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Edge->SetBrushColor(Palette().Blood);
		Edge->SetPadding(FMargin(1.0f));
		UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Edge->SetContent(Fill);
		CellBox->SetContent(Edge);
		UHorizontalBoxSlot* CellSlot = ThreatBox->AddChildToHorizontalBox(CellBox);
		CellSlot->SetVerticalAlignment(VAlign_Center);
		CellSlot->SetPadding(FMargin(Index == 0 ? 5.0f : 3.0f, 0.0f, 0.0f, 0.0f));
		ThreatCells.Add(Fill);
	}

	ForecastText = MakeText(15, Palette().Mist, false);
	ForecastText->SetAutoWrapText(true);
	UHorizontalBoxSlot* ForecastSlot = Next->AddChildToHorizontalBox(ForecastText);
	ForecastSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ForecastSlot->SetVerticalAlignment(VAlign_Center);
	ForecastSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));

	LaunchButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LaunchButton"));
	LaunchButton->SetBackgroundColor(Palette().Ember);
	LaunchButton->OnClicked.AddDynamic(this, &UCytraHudWidget::HandleLaunchClicked);
	LaunchLabel = MakeText(16, Palette().Rock950, true);
	LaunchButton->SetContent(LaunchLabel);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(LaunchButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);

	AddControl(Row, PauseLabel, [this]()
	{
		if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
		{
			Controller->TogglePause();
		}
	});
	AddControl(Row, SpeedLabel, [this]()
	{
		if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
		{
			Controller->CycleSpeed();
		}
	});
	AddControl(Row, RestartLabel, [this]()
	{
		// A first click asks for a confirmation, a second one within 3 seconds restarts.
		const double Now = FPlatformTime::Seconds();
		if (Now < RestartArmedUntil)
		{
			if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
			{
				Controller->RestartMatch();
			}
			return;
		}
		RestartArmedUntil = Now + RestartConfirmSeconds;
		RestartLabel->SetText(CytraText::Get(TEXT("Hud.RestartConfirm")));
	});
	RestartLabel->SetText(CytraText::Get(TEXT("Hud.Restart")));
}

void UCytraHudWidget::BuildBoardArea(UOverlay* Area)
{
	// The message, at the top of the board, centred: a 1-pixel edge colored by its tone around a dark box.
	ToastFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToastFrame"));
	ToastFrame->SetPadding(FMargin(1.0f));
	ToastFrame->SetVisibility(ESlateVisibility::Collapsed);
	ToastBack = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ToastBack"));
	ToastBack->SetBrushColor(WithAlpha(Palette().Rock950, 0.92f));
	ToastBack->SetPadding(FMargin(14.0f, 7.0f));
	ToastFrame->SetContent(ToastBack);
	ToastText = MakeText(15, Palette().Snow, false);
	ToastText->SetJustification(ETextJustify::Center);
	ToastText->SetAutoWrapText(true);
	ToastBack->SetContent(ToastText);
	UOverlaySlot* ToastSlot = Area->AddChildToOverlay(ToastFrame);
	ToastSlot->SetHorizontalAlignment(HAlign_Center);
	ToastSlot->SetVerticalAlignment(VAlign_Top);
	ToastSlot->SetPadding(FMargin(12.0f, 12.0f, 12.0f, 0.0f));

	// The end screen darkens the board and stops clicks on it; the panel stays usable.
	EndScreen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EndScreen"));
	EndScreen->SetBrushColor(WithAlpha(Palette().Rock950, 0.86f));
	EndScreen->SetPadding(FMargin(24.0f));
	EndScreen->SetHorizontalAlignment(HAlign_Center);
	EndScreen->SetVerticalAlignment(VAlign_Center);
	EndScreen->SetVisibility(ESlateVisibility::Collapsed);
	EndScreen->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UCytraHudWidget, HandleEndMouseDown));
	UOverlaySlot* EndSlot = Area->AddChildToOverlay(EndScreen);
	EndSlot->SetHorizontalAlignment(HAlign_Fill);
	EndSlot->SetVerticalAlignment(VAlign_Fill);

	// About 46 characters wide, as in the prototype.
	USizeBox* EndWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EndWidth->SetMaxDesiredWidth(440.0f);
	EndScreen->SetContent(EndWidth);
	UVerticalBox* EndContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EndContent"));
	EndWidth->SetContent(EndContent);

	auto AddEndText = [this, EndContent](int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = MakeText(Size, Color, false);
		Text->SetAutoWrapText(true);
		Text->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TextSlot = EndContent->AddChildToVerticalBox(Text);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		return Text;
	};
	EndTitle = AddEndText(30, Palette().Snow);
	EndBody = AddEndText(15, Palette().Mist);
	EndProgress = AddEndText(15, Palette().Mist);
	EndStats = AddEndText(15, Palette().Mist);

	UCytraPanelButton* Again = WidgetTree->ConstructWidget<UCytraPanelButton>(UCytraPanelButton::StaticClass(), TEXT("AgainButton"));
	Again->SetBackgroundColor(Palette().Ember);
	Again->SetCallback([this]()
	{
		if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
		{
			Controller->RestartMatch();
		}
	});
	UTextBlock* AgainLabel = MakeText(16, Palette().Rock950, true);
	AgainLabel->SetText(CytraText::Get(TEXT("End.Again")));
	Again->SetContent(AgainLabel);
	EndContent->AddChildToVerticalBox(Again)->SetHorizontalAlignment(HAlign_Center);
}

UTextBlock* UCytraHudWidget::AddStat(UHorizontalBox* Row, const TCHAR* LabelKey, const FLinearColor& ValueColor)
{
	UVerticalBox* Stat = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Label = MakeText(12, Palette().Mist, false);
	Label->SetText(CytraText::Get(LabelKey).ToUpper());
	Stat->AddChildToVerticalBox(Label);
	UTextBlock* Value = MakeText(26, ValueColor, true);
	Stat->AddChildToVerticalBox(Value);

	UHorizontalBoxSlot* StatSlot = Row->AddChildToHorizontalBox(Stat);
	StatSlot->SetVerticalAlignment(VAlign_Center);
	StatSlot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));
	return Value;
}

UCytraPanelButton* UCytraHudWidget::AddControl(UHorizontalBox* Row, TObjectPtr<UTextBlock>& OutLabel, TFunction<void()> Callback)
{
	UCytraPanelButton* Button = WidgetTree->ConstructWidget<UCytraPanelButton>(UCytraPanelButton::StaticClass());
	Button->SetBackgroundColor(Palette().Rock700);
	Button->SetCallback(MoveTemp(Callback));
	OutLabel = MakeText(16, Palette().Snow, false);
	Button->SetContent(OutLabel);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(Button);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	return Button;
}

UTextBlock* UCytraHudWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

void UCytraHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ACytraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr)
	{
		MatchChangedHandle = GameState->OnMatchChanged.AddUObject(this, &UCytraHudWidget::Refresh);
	}
	if (ACytraGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACytraGameMode>() : nullptr)
	{
		MessageHandle = GameMode->OnMessage.AddUObject(this, &UCytraHudWidget::ShowMessage);
	}
	Refresh();
}

void UCytraHudWidget::NativeDestruct()
{
	if (ACytraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr)
	{
		GameState->OnMatchChanged.Remove(MatchChangedHandle);
	}
	if (ACytraGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACytraGameMode>() : nullptr)
	{
		GameMode->OnMessage.Remove(MessageHandle);
	}
	MatchChangedHandle.Reset();
	MessageHandle.Reset();
	Super::NativeDestruct();
}

void UCytraHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const double Now = FPlatformTime::Seconds();
	if (ToastFrame->GetVisibility() != ESlateVisibility::Collapsed && Now >= ToastHideTime)
	{
		ToastFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RestartArmedUntil > 0.0 && Now >= RestartArmedUntil)
	{
		RestartArmedUntil = 0.0;
		RestartLabel->SetText(CytraText::Get(TEXT("Hud.Restart")));
	}
	PlaceMapText();
}

void UCytraHudWidget::Refresh()
{
	const ACytraGameState* State = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr;
	if (!State)
	{
		return;
	}

	GoldValue->SetText(FText::AsNumber(State->Gold));
	EggsValue->SetText(FText::AsNumber(State->Eggs));
	WaveValue->SetText(FText::Format(CytraText::Get(TEXT("Hud.WaveValue")), State->Wave, State->WaveCount));

	FText Next;
	FText Launch;
	bool bCanLaunch = false;
	switch (State->Phase)
	{
	case ECytraMatchPhase::Prep:
		Next = State->bHasCountdown
			? FText::Format(CytraText::Get(TEXT("Hud.NextIn")), State->Wave + 1, State->CountdownSeconds)
			: FText::Format(CytraText::Get(TEXT("Hud.Next")), State->Wave + 1);
		Launch = State->bHasCountdown
			? FText::Format(CytraText::Get(TEXT("Hud.LaunchNow")), State->EarlyLaunchBonus)
			: FText::Format(CytraText::Get(TEXT("Hud.LaunchFirst")), State->Wave + 1);
		bCanLaunch = true;
		break;
	case ECytraMatchPhase::Wave:
		Next = FText::Format(CytraText::Get(TEXT("Hud.WaveRunning")), State->Wave, State->EnemiesLeft);
		Launch = CytraText::Get(TEXT("Hud.WaveInProgress"));
		break;
	case ECytraMatchPhase::Won:
		Next = CytraText::Get(TEXT("Hud.Won"));
		Launch = CytraText::Get(TEXT("Hud.GameOver"));
		break;
	case ECytraMatchPhase::Lost:
		Next = CytraText::Get(TEXT("Hud.Lost"));
		Launch = CytraText::Get(TEXT("Hud.GameOver"));
		break;
	}

	NextText->SetText(Next);
	LaunchLabel->SetText(Launch);
	LaunchLabel->SetColorAndOpacity(FSlateColor(bCanLaunch ? Palette().Rock950 : Palette().Mist));
	LaunchButton->SetBackgroundColor(bCanLaunch ? Palette().Ember : Palette().Rock700);
	LaunchButton->SetIsEnabled(bCanLaunch);

	// Threat and forecast of the next wave, during preparation only.
	const ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>();
	const FCytraMatch* Match = GameMode ? GameMode->GetMatch() : nullptr;
	const bool bForecast = Match && State->Phase == ECytraMatchPhase::Prep && State->Wave < State->WaveCount;
	ThreatBox->SetVisibility(bForecast ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ForecastText->SetVisibility(bForecast ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bForecast)
	{
		const int32 Level = Match->GetWaves().ThreatLevel(State->Wave, Match->GetBalance());
		for (int32 Index = 0; Index < ThreatCells.Num(); ++Index)
		{
			ThreatCells[Index]->SetBrushColor(Index < Level ? Palette().Blood : Palette().Rock900);
		}
		ForecastText->SetText(FCytraHudView::Forecast(*Match, State->Wave));
	}

	PauseLabel->SetText(CytraText::Get(State->bMatchPaused ? TEXT("Hud.Resume") : TEXT("Hud.Pause")));
	PauseLabel->SetColorAndOpacity(FSlateColor(State->bMatchPaused ? Palette().Ember : Palette().Snow));
	SpeedLabel->SetText(FText::Format(CytraText::Get(TEXT("Hud.Speed")), State->Speed));

	const bool bOver = State->Phase == ECytraMatchPhase::Won || State->Phase == ECytraMatchPhase::Lost;
	if (bOver && EndScreen->GetVisibility() == ESlateVisibility::Collapsed)
	{
		ShowEnd();
	}
	else if (!bOver)
	{
		EndScreen->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UCytraHudWidget::ShowEnd()
{
	// Written once, when the match ends, as in the prototype.
	const ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>();
	const FCytraMatch* Match = GameMode ? GameMode->GetMatch() : nullptr;
	const TOptional<FCytraEndView> View = Match ? FCytraEndView::Build(*Match) : TOptional<FCytraEndView>();
	if (!View)
	{
		return;
	}
	EndTitle->SetText(View->Title);
	EndBody->SetText(View->Body);
	EndProgress->SetText(View->Progress);
	EndStats->SetText(View->Stats);
	EndScreen->SetVisibility(ESlateVisibility::Visible);
}

void UCytraHudWidget::ShowMessage(const FCytraMessage& Message)
{
	// One message at a time: a new one replaces the last, as in the prototype.
	const bool bWarning = Message.Tone == ECytraMessageTone::Warning;
	ToastText->SetText(Message.Text);
	ToastText->SetColorAndOpacity(FSlateColor(bWarning ? Palette().WarningMessage : Palette().Snow));
	ToastFrame->SetBrushColor(bWarning ? Palette().Blood : Palette().Moss);
	ToastFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastHideTime = FPlatformTime::Seconds() + ToastSeconds;
}

void UCytraHudWidget::PlaceMapText()
{
	const ACytraGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACytraGameMode>() : nullptr;
	const ACytraBoard* Board = GameMode ? GameMode->GetBoard() : nullptr;
	APlayerController* Controller = GetOwningPlayer();
	if (!Board || !Controller)
	{
		return;
	}
	const FCytraMapDef& Map = Board->GetMap();
	auto Project = [Board, Controller, &Map](const FVector2d& At, FVector2D& OutPosition)
	{
		const FIntPoint Cell(FMath::RoundToInt32(At.X), FMath::RoundToInt32(At.Y));
		const FVector World = Board->GridToWorld(At) + FVector(0.0f, 0.0f, Board->GetTileHeight(Map.Get(Cell)));
		return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, World, OutPosition, false);
	};

	// Sizes follow the board on screen: the prototype draws them on 40-pixel cells.
	FVector2D Origin;
	FVector2D East;
	if (!Project(FVector2d(0.0, 0.0), Origin) || !Project(FVector2d(1.0, 0.0), East))
	{
		return;
	}
	const float CellSize = static_cast<float>(FVector2D::Distance(Origin, East));
	const float Scale = CellSize / 40.0f;
	bool bResized = !FMath::IsNearlyEqual(CellSize, MapTextCellSize, 0.5f);

	auto SetFontSize = [Scale](UTextBlock* Text, float Pixels, bool bBold)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FMath::Max(6.0f, Pixels * Scale * PointsPerPixel);
		Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
		// The prototype strokes its labels 4 pixels wide: 2 outside the letters.
		Font.OutlineSettings.OutlineSize = bBold ? 0 : FMath::Max(1, FMath::RoundToInt32(2.0f * Scale));
		Font.OutlineSettings.OutlineColor = WithAlpha(Palette().Rock950, 0.85f);
		Text->SetFont(Font);
	};

	// Map names, created once.
	if (MapLabels.Num() != Map.Labels.Num())
	{
		for (UTextBlock* Text : MapLabels)
		{
			Text->RemoveFromParent();
		}
		MapLabels.Reset();
		for (const FCytraMapLabel& Label : Map.Labels)
		{
			UTextBlock* Text = MakeText(15, Palette().Snow, false);
			Text->SetText(CytraText::Get(FString::Printf(TEXT("Label.%s"), *Label.Id.ToString())));
			UCanvasPanelSlot* TextSlot = MapText->AddChildToCanvas(Text);
			TextSlot->SetAutoSize(true);
			const float AlignX = Label.Align == ECytraLabelAlign::Left ? 0.0f : Label.Align == ECytraLabelAlign::Center ? 0.5f : 1.0f;
			TextSlot->SetAlignment(FVector2D(AlignX, 0.5f));
			MapLabels.Add(Text);
		}
		bResized = true;
	}
	for (int32 Index = 0; Index < MapLabels.Num(); ++Index)
	{
		FVector2D Position;
		if (Project(Map.Labels[Index].At, Position))
		{
			if (bResized)
			{
				SetFontSize(MapLabels[Index], static_cast<float>(Map.Labels[Index].Size), false);
			}
			Cast<UCanvasPanelSlot>(MapLabels[Index]->Slot)->SetPosition(Position);
		}
	}
	MapTextCellSize = CellSize;

	// Gold earned: "+6" rising and fading over the point where it was earned.
	const TArray<FCytraFloatingGold>& Golds = GameMode->GetFloatingGold();
	while (GoldTexts.Num() < Golds.Num())
	{
		UTextBlock* Text = MakeText(13, Palette().Gold, true);
		Text->SetJustification(ETextJustify::Center);
		UCanvasPanelSlot* TextSlot = MapText->AddChildToCanvas(Text);
		TextSlot->SetAutoSize(true);
		TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		SetFontSize(Text, GoldTextSize, true);
		GoldTexts.Add(Text);
	}
	for (int32 Index = 0; Index < GoldTexts.Num(); ++Index)
	{
		UTextBlock* Text = GoldTexts[Index];
		FVector2D Position;
		if (!Golds.IsValidIndex(Index) || !Project(Golds[Index].At, Position))
		{
			Text->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FCytraFloatingGold& Gold = Golds[Index];
		const float Ratio = Gold.Duration > 0.0f ? FMath::Clamp(Gold.Age / Gold.Duration, 0.0f, 1.0f) : 1.0f;
		// North of the point is up on screen.
		Project(Gold.At - FVector2d(0.0, GoldRise + GoldRiseMore * Ratio), Position);
		if (bResized)
		{
			SetFontSize(Text, GoldTextSize, true);
		}
		Text->SetText(FText::Format(INVTEXT("+{0}"), Gold.Gold));
		Text->SetColorAndOpacity(FSlateColor(WithAlpha(Palette().Gold, 1.0f - Ratio)));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Cast<UCanvasPanelSlot>(Text->Slot)->SetPosition(Position);
	}
}

void UCytraHudWidget::HandleLaunchClicked()
{
	if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
	{
		Controller->LaunchWave();
	}
}

FEventReply UCytraHudWidget::HandleEndMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Handled();
}
