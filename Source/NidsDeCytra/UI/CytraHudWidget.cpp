// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Game/CytraGameState.h"
#include "Game/CytraPlayerController.h"
#include "UI/CytraLayout.h"
#include "UI/CytraOrderPanel.h"
#include "UI/CytraPalette.h"
#include "UI/CytraText.h"

namespace
{
	const FCytraPalette& Palette()
	{
		return FCytraPalette::Get();
	}
}

void UCytraHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// The HUD covers the whole screen: only the bar and the panel may catch clicks, the rest goes through to the board.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* Bar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bar"));
	Bar->SetBrushColor(Palette().Rock900);
	Bar->SetPadding(FMargin(14.0f, 10.0f));
	UVerticalBoxSlot* BarSlot = Root->AddChildToVerticalBox(Bar);
	BarSlot->SetPadding(FMargin(12.0f));
	BarSlot->SetHorizontalAlignment(HAlign_Fill);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	Bar->SetContent(Row);

	GoldValue = AddStat(Row, TEXT("Hud.Gold"), Palette().Gold);
	EggsValue = AddStat(Row, TEXT("Hud.Eggs"), Palette().Silk);
	WaveValue = AddStat(Row, TEXT("Hud.Wave"), Palette().Snow);

	NextText = MakeText(15, Palette().Mist, false);
	NextText->SetAutoWrapText(true);
	UHorizontalBoxSlot* NextSlot = Row->AddChildToHorizontalBox(NextText);
	NextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NextSlot->SetVerticalAlignment(VAlign_Center);
	NextSlot->SetPadding(FMargin(0.0f, 0.0f, 22.0f, 0.0f));

	LaunchButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LaunchButton"));
	LaunchButton->SetBackgroundColor(Palette().Ember);
	LaunchButton->OnClicked.AddDynamic(this, &UCytraHudWidget::HandleLaunchClicked);
	LaunchLabel = MakeText(16, Palette().Rock950, true);
	LaunchButton->SetContent(LaunchLabel);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(LaunchButton);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);

	// Under the bar, the order panel on the right; the camera keeps the board clear of both.
	UHorizontalBox* Below = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Below"));
	Below->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBoxSlot* BelowSlot = Root->AddChildToVerticalBox(Below);
	BelowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Spacer->SetVisibility(ESlateVisibility::HitTestInvisible);
	Below->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

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
	Refresh();
}

void UCytraHudWidget::NativeDestruct()
{
	if (ACytraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr)
	{
		GameState->OnMatchChanged.Remove(MatchChangedHandle);
	}
	MatchChangedHandle.Reset();
	Super::NativeDestruct();
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
}

void UCytraHudWidget::HandleLaunchClicked()
{
	if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
	{
		Controller->LaunchWave();
	}
}
