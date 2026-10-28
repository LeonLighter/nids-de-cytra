// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraOrderPanel.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Game/CytraGameMode.h"
#include "Game/CytraGameState.h"
#include "Game/CytraPlayerController.h"
#include "TimerManager.h"
#include "UI/CytraPalette.h"

namespace
{
	/** Space between the blocks of the panel, as in the prototype. */
	constexpr float BlockGap = 10.0f;
	/** Space between the order card and the crew card. */
	constexpr float CardGap = 12.0f;
	/** Opacity of a refused order. */
	constexpr float RefusedOpacity = 0.62f;
}

void UCytraPanelButton::SetCallback(TFunction<void()> InCallback)
{
	Callback = MoveTemp(InCallback);
	OnClicked.AddUniqueDynamic(this, &UCytraPanelButton::HandleClicked);
}

void UCytraPanelButton::HandleClicked()
{
	if (Callback)
	{
		Callback();
	}
}

void UCytraPanelButton::SetHoverCallback(TFunction<void(bool)> InCallback)
{
	HoverCallback = MoveTemp(InCallback);
	OnHovered.AddUniqueDynamic(this, &UCytraPanelButton::HandleHovered);
	OnUnhovered.AddUniqueDynamic(this, &UCytraPanelButton::HandleUnhovered);
}

void UCytraPanelButton::HandleHovered()
{
	if (HoverCallback)
	{
		HoverCallback(true);
	}
}

void UCytraPanelButton::HandleUnhovered()
{
	if (HoverCallback)
	{
		HoverCallback(false);
	}
}

void UCytraOrderPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Two cards in a column, as in the prototype: the orders for the selected cell, and the crew below.
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Column;
	Content = AddCard(Column, TEXT("Orders"));
	CrewContent = AddCard(Column, TEXT("Crew"));
}

UVerticalBox* UCytraOrderPanel::AddCard(UVerticalBox* Column, const TCHAR* Name)
{
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(FString(Name) + TEXT("Background")));
	Background->SetBrushColor(FCytraPalette::Get().Rock900);
	Background->SetPadding(FMargin(14.0f));
	Background->OnMouseButtonDownEvent.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UCytraOrderPanel, HandleBackgroundMouseDown));
	UVerticalBoxSlot* CardSlot = Column->AddChildToVerticalBox(Background);
	CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, CardGap));

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(Name));
	Background->SetContent(Box);
	return Box;
}

void UCytraOrderPanel::ShowCard(UVerticalBox* Box, const FCytraPanelView& View)
{
	Box->ClearChildren();
	AddTitle(Box, View);
	int32 StepNumber = 0;
	for (const FCytraPanelLine& Line : View.Lines)
	{
		StepNumber = Line.Style == ECytraPanelLineStyle::Step ? StepNumber + 1 : 0;
		AddLine(Box, Line, StepNumber);
	}
	for (const FCytraPanelAction& Action : View.Actions)
	{
		AddAction(Box, Action);
	}
}

void UCytraOrderPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (ACytraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr)
	{
		MatchChangedHandle = GameState->OnMatchChanged.AddUObject(this, &UCytraOrderPanel::QueueRefresh);
	}
	if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
	{
		SelectionChangedHandle = Controller->OnSelectionChanged.AddUObject(this, &UCytraOrderPanel::QueueRefresh);
	}
	Refresh();
}

void UCytraOrderPanel::NativeDestruct()
{
	if (ACytraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACytraGameState>() : nullptr)
	{
		GameState->OnMatchChanged.Remove(MatchChangedHandle);
	}
	if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
	{
		Controller->OnSelectionChanged.Remove(SelectionChangedHandle);
	}
	MatchChangedHandle.Reset();
	SelectionChangedHandle.Reset();
	Super::NativeDestruct();
}

void UCytraOrderPanel::QueueRefresh()
{
	UWorld* World = GetWorld();
	if (bRefreshQueued || !World)
	{
		return;
	}
	bRefreshQueued = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bRefreshQueued = false;
		Refresh();
	}));
}

void UCytraOrderPanel::Refresh()
{
	const ACytraGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ACytraGameMode>() : nullptr;
	const FCytraMatch* Match = GameMode ? GameMode->GetMatch() : nullptr;
	ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>();
	if (!Match || !Controller || !Content || !CrewContent)
	{
		return;
	}

	const FCytraPanelView View = FCytraPanelView::Build(*Match, Controller->GetSelectedCell());
	FString Signature = View.Signature();
	if (Signature != ShownSignature)
	{
		ShownSignature = MoveTemp(Signature);
		// The buttons are rebuilt: the one under the cursor may never report that it was left.
		Controller->PreviewTower(NAME_None);
		ShowCard(Content, View);
	}

	const FCytraPanelView Crew = FCytraPanelView::BuildCrew(*Match);
	FString CrewSignature = Crew.Signature();
	if (CrewSignature != ShownCrewSignature)
	{
		ShownCrewSignature = MoveTemp(CrewSignature);
		ShowCard(CrewContent, Crew);
	}
}

void UCytraOrderPanel::AddTitle(UVerticalBox* Box, const FCytraPanelView& View)
{
	const FCytraPalette& Palette = FCytraPalette::Get();
	UTextBlock* Title = MakeText(21, Palette.Snow, false, View.Title);
	if (View.TitleNote.IsEmpty())
	{
		Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BlockGap));
	}
	else
	{
		Title->SetAutoWrapText(false);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Bottom);
		UHorizontalBoxSlot* NoteSlot = Row->AddChildToHorizontalBox(MakeText(14, Palette.Mist, false, View.TitleNote));
		NoteSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 3.0f));
		NoteSlot->SetVerticalAlignment(VAlign_Bottom);
		Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BlockGap));
	}
	if (!View.Tag.IsEmpty())
	{
		UTextBlock* Tag = MakeText(12, Palette.Snow, false, View.Tag.ToUpper());
		Box->AddChildToVerticalBox(Tag)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BlockGap));
	}
	for (int32 Index = 0; Index < View.Stats.Num(); ++Index)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->AddChildToHorizontalBox(MakeText(15, Palette.Mist, false, View.Stats[Index].Label))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Value = MakeText(15, Palette.Snow, true, View.Stats[Index].Value);
		Value->SetAutoWrapText(false);
		Row->AddChildToHorizontalBox(Value);
		const float Gap = Index == View.Stats.Num() - 1 ? BlockGap : 4.0f;
		Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Gap));
	}
}

void UCytraOrderPanel::AddLine(UVerticalBox* Box, const FCytraPanelLine& Line, int32 StepNumber)
{
	const FCytraPalette& Palette = FCytraPalette::Get();
	if (Line.Style != ECytraPanelLineStyle::Step)
	{
		const FLinearColor& Color = Line.Style == ECytraPanelLineStyle::State ? Palette.Snow
			: Line.Style == ECytraPanelLineStyle::Warning ? Palette.Warning
			: Palette.Mist;
		Box->AddChildToVerticalBox(MakeText(15, Color, false, Line.Text))->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BlockGap));
		return;
	}

	// A numbered step, its text a link when it names a cell to open.
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->AddChildToHorizontalBox(MakeText(15, Palette.Mist, false, FText::Format(INVTEXT("{0}."), StepNumber)))->SetPadding(FMargin(4.0f, 0.0f, 8.0f, 0.0f));
	if (Line.Link)
	{
		const FIntPoint Cell = *Line.Link;
		UCytraPanelButton* Link = MakeButton(FLinearColor::Transparent, [this, Cell]()
		{
			if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
			{
				Controller->SelectCell(Cell);
			}
		});
		UTextBlock* LinkText = MakeText(15, Palette.Ember, false, Line.Text);
		LinkText->SetAutoWrapText(false);
		Cast<UButtonSlot>(Link->SetContent(LinkText))->SetPadding(FMargin(0.0f));
		Row->AddChildToHorizontalBox(Link)->SetVerticalAlignment(VAlign_Top);
		UHorizontalBoxSlot* SuffixSlot = Row->AddChildToHorizontalBox(MakeText(15, Palette.Mist, false, Line.Suffix));
		SuffixSlot->SetPadding(FMargin(5.0f, 0.0f, 0.0f, 0.0f));
		SuffixSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	else
	{
		Row->AddChildToHorizontalBox(MakeText(15, Palette.Snow, false, Line.Text))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
}

void UCytraOrderPanel::AddAction(UVerticalBox* Box, const FCytraPanelAction& Action)
{
	const FCytraPalette& Palette = FCytraPalette::Get();
	const FCytraOrderAction Order = Action.Action;
	const FName Recruit = Action.Recruit;
	UCytraPanelButton* Button = MakeButton(Palette.Rock800, [this, Order, Recruit]()
	{
		if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
		{
			if (Recruit.IsNone())
			{
				Controller->OrderOnSelection(Order);
			}
			else
			{
				Controller->Recruit(Recruit);
			}
		}
		QueueRefresh();
	});

	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Header->AddChildToHorizontalBox(MakeText(16, Palette.Snow, true, Action.Name))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* Cost = MakeText(16, Palette.Gold, true, Action.Cost);
	Cost->SetAutoWrapText(false);
	Header->AddChildToHorizontalBox(Cost)->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
	Body->AddChildToVerticalBox(Header);
	Body->AddChildToVerticalBox(MakeText(14, Palette.Mist, false, Action.Meta));
	if (!Action.IsEnabled())
	{
		// Dimmed but not disabled, as in the prototype: it still shows the range of its tower under the cursor,
		// and a click is refused by the rules.
		Body->AddChildToVerticalBox(MakeText(14, Palette.Warning, false, Action.Refusal));
		Button->SetRenderOpacity(RefusedOpacity);
	}
	if (Recruit.IsNone() && Order.Kind == ECytraOrderKind::Build)
	{
		const FName Tower = Order.Id;
		Button->SetHoverCallback([this, Tower](bool bHovered)
		{
			if (ACytraPlayerController* Controller = GetOwningPlayer<ACytraPlayerController>())
			{
				Controller->PreviewTower(bHovered ? Tower : NAME_None);
			}
		});
	}

	Cast<UButtonSlot>(Button->SetContent(Body))->SetPadding(FMargin(12.0f, 9.0f));
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button);
	ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
}

UTextBlock* UCytraOrderPanel::MakeText(int32 Size, const FLinearColor& Color, bool bBold, const FText& Text)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Font.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetAutoWrapText(true);
	Block->SetText(Text);
	return Block;
}

UCytraPanelButton* UCytraOrderPanel::MakeButton(const FLinearColor& Background, TFunction<void()> Callback)
{
	UCytraPanelButton* Button = WidgetTree->ConstructWidget<UCytraPanelButton>(UCytraPanelButton::StaticClass());
	Button->SetBackgroundColor(Background);
	Button->SetCallback(MoveTemp(Callback));
	return Button;
}

FEventReply UCytraOrderPanel::HandleBackgroundMouseDown(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	return UWidgetBlueprintLibrary::Handled();
}
