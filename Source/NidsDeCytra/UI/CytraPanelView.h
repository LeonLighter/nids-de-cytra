// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraOrders.h"

class FCytraMatch;

enum class ECytraPanelLineStyle : uint8
{
	/** Description, in a muted color. */
	Text,
	/** What is going on with the job on the cell. */
	State,
	Warning,
	/** A numbered step. */
	Step,
};

struct FCytraPanelLine
{
	ECytraPanelLineStyle Style = ECytraPanelLineStyle::Text;
	FText Text;
	/** Access chain step: clicking the text selects this cell. */
	TOptional<FIntPoint> Link;
	/** Shown after a link. */
	FText Suffix;
};

/** A labelled value, such as the damage of a tower. */
struct FCytraPanelStat
{
	FText Label;
	FText Value;
};

/** An order the player can give on the selected cell. */
struct FCytraPanelAction
{
	FCytraOrderAction Action;
	FText Name;
	FText Cost;
	/** Specialty and job duration. */
	FText Meta;
	/** Why the order is refused, empty if it is accepted. */
	FText Refusal;

	bool IsEnabled() const { return Refusal.IsEmpty(); }
};

/**
 * Content of the order panel for the selected cell, after selHTML() in the web prototype.
 * Built from the match alone, so tests can check it without any widget.
 */
struct NIDSDECYTRA_API FCytraPanelView
{
	FText Title;
	/** Small text after the title, such as the level of a tower. Empty if none. */
	FText TitleNote;
	/** Small label under the title, such as the range bonus of a promontory. Empty if none. */
	FText Tag;
	TArray<FCytraPanelStat> Stats;
	TArray<FCytraPanelLine> Lines;
	TArray<FCytraPanelAction> Actions;

	/** The panel for Cell, or the help text when nothing is selected. */
	static FCytraPanelView Build(const FCytraMatch& Match, const TOptional<FIntPoint>& Cell);

	/** Player-facing reason of a refused order. */
	static FText RefusalText(const FCytraOrderCheck& Check);

	/** Equal for equal contents: the panel redraws only when it changes. */
	FString Signature() const;
};
