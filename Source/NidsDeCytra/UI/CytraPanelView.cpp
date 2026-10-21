// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraPanelView.h"

#include "Rules/CytraMatch.h"
#include "UI/CytraText.h"

namespace
{
	FString TileKey(ECytraTileType Type, const TCHAR* Field)
	{
		return FString::Printf(TEXT("Tile.%s.%s"), *StaticEnum<ECytraTileType>()->GetNameStringByValue(static_cast<int64>(Type)), Field);
	}

	FText TileText(ECytraTileType Type, const TCHAR* Field)
	{
		return CytraText::Get(TileKey(Type, Field));
	}

	FText RoleShort(FName Role)
	{
		return CytraText::Get(FString::Printf(TEXT("Role.%s.Short"), *Role.ToString()));
	}

	FText WorkText(FName Work, const TCHAR* Field)
	{
		return CytraText::Get(FString::Printf(TEXT("Work.%s.%s"), *Work.ToString(), Field));
	}

	/** Works that open this tile type, in data order. */
	TArray<const FCytraWorkDef*> WorksOn(const FCytraBalance& Balance, ECytraTileType Type)
	{
		TArray<const FCytraWorkDef*> Works;
		for (const TPair<FName, FCytraWorkDef>& Work : Balance.Works)
		{
			if (Work.Value.On == Type)
			{
				Works.Add(&Work.Value);
			}
		}
		return Works;
	}

	/** Who is on the job: nobody yet, a worker on the way, or a worker on site. */
	FText JobLine(const FCytraMatch& Match, const FCytraJob& Job)
	{
		const FCytraWorker* Worker = Match.FindWorker(Job.Worker);
		if (!Worker)
		{
			return FText::Format(CytraText::Get(TEXT("Panel.JobWaiting")), RoleShort(Job.Role));
		}
		return Worker->State == ECytraWorkerState::Move
			? FText::Format(CytraText::Get(TEXT("Panel.JobComing")), RoleShort(Job.Role))
			: CytraText::Get(TEXT("Panel.JobRunning"));
	}

	void AddLine(FCytraPanelView& View, ECytraPanelLineStyle Style, const FText& Text)
	{
		FCytraPanelLine& Line = View.Lines.AddDefaulted_GetRef();
		Line.Style = Style;
		Line.Text = Text;
	}

	/** The job ordered on the cell, or a button for each work that applies there. */
	void AddWorks(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, ECytraTileType Type)
	{
		if (const FCytraJob* Job = Match.FindJobAt(Cell))
		{
			AddLine(View, ECytraPanelLineStyle::State, FText::Format(CytraText::Get(TEXT("Panel.JobState")), WorkText(Job->Type, TEXT("Ordered")), JobLine(Match, *Job)));
			return;
		}
		for (const FCytraWorkDef* Work : WorksOn(Match.GetBalance(), Type))
		{
			const FCytraOrderAction Action = FCytraOrderAction::Work(Work->Id);
			const FCytraOrderCheck Check = Match.CheckOrder(Cell, Action);
			if (!Check.Spec)
			{
				continue;
			}
			FCytraPanelAction& Entry = View.Actions.AddDefaulted_GetRef();
			Entry.Action = Action;
			Entry.Name = WorkText(Work->Id, TEXT("Label"));
			Entry.Cost = FText::Format(CytraText::Get(TEXT("Panel.Cost")), Check.Spec->Cost);
			Entry.Meta = FText::Format(CytraText::Get(TEXT("Panel.Meta")), RoleShort(Check.Spec->Role), FText::AsNumber(Check.Spec->Time));
			if (!Check.IsAccepted())
			{
				Entry.Refusal = FCytraPanelView::RefusalText(Check);
			}
		}
	}

	void BuildSlot(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, ECytraTileType Type)
	{
		const FCytraGrid& Grid = Match.GetGrid();
		View.Title = TileText(Type, TEXT("Title"));
		if (Type == ECytraTileType::HighSlot)
		{
			const int32 Bonus = FMath::RoundToInt32((Match.GetBalance().HighSlotRangeBonus - 1.0) * 100.0);
			View.Tag = FText::Format(CytraText::Get(TEXT("Panel.HighTag")), Bonus);
		}
		// Towers are built, upgraded and dismantled from here (J40).
		if (Grid.IsReachable(Cell))
		{
			return;
		}
		AddLine(View, ECytraPanelLineStyle::Warning, CytraText::Get(TEXT("Panel.CutOff")));
		for (const FIntPoint& Obstacle : Grid.AccessChain(Cell))
		{
			const TArray<const FCytraWorkDef*> Works = WorksOn(Match.GetBalance(), Grid.GetType(Obstacle));
			if (Works.IsEmpty())
			{
				continue;
			}
			FCytraPanelLine& Line = View.Lines.AddDefaulted_GetRef();
			Line.Style = ECytraPanelLineStyle::Step;
			Line.Text = WorkText(Works[0]->Id, TEXT("Label"));
			Line.Link = Obstacle;
			Line.Suffix = FText::Format(CytraText::Get(TEXT("Panel.ChainStep")), RoleShort(Works[0]->Role), Works[0]->Cost);
		}
	}
}

FCytraPanelView FCytraPanelView::Build(const FCytraMatch& Match, const TOptional<FIntPoint>& Cell)
{
	FCytraPanelView View;
	if (!Cell || !Match.GetMap().IsInside(*Cell))
	{
		View.Title = CytraText::Get(TEXT("Panel.Title"));
		AddLine(View, ECytraPanelLineStyle::Text, CytraText::Get(TEXT("Panel.Hint")));
		for (const TCHAR* Key : { TEXT("Panel.Hint1"), TEXT("Panel.Hint2"), TEXT("Panel.Hint3"), TEXT("Panel.Hint4") })
		{
			AddLine(View, ECytraPanelLineStyle::Step, CytraText::Get(Key));
		}
		return View;
	}

	const FCytraGrid& Grid = Match.GetGrid();
	const ECytraTileType Type = Grid.GetType(*Cell);
	const bool bOpened = Grid.IsOpened(*Cell);

	if (IsCytraSlot(Type))
	{
		BuildSlot(View, Match, *Cell, Type);
	}
	else if (IsCytraObstacle(Type) && bOpened)
	{
		View.Title = TileText(Type, TEXT("Opened"));
		AddLine(View, ECytraPanelLineStyle::Text, CytraText::Get(TEXT("Tile.Opened.Desc")));
	}
	else if (Type == ECytraTileType::OutpostSite && bOpened)
	{
		View.Title = TileText(Type, TEXT("Opened"));
		AddLine(View, ECytraPanelLineStyle::Text, TileText(Type, TEXT("OpenedDesc")));
	}
	else if (IsCytraObstacle(Type) || Type == ECytraTileType::OutpostSite)
	{
		View.Title = TileText(Type, TEXT("Title"));
		AddLine(View, ECytraPanelLineStyle::Text, TileText(Type, TEXT("Desc")));
		AddWorks(View, Match, *Cell, Type);
	}
	else if (Type == ECytraTileType::Mine)
	{
		// Opening and working the mine come in J50.
		const FCytraBalance& Balance = Match.GetBalance();
		View.Title = TileText(Type, TEXT("Title"));
		AddLine(View, ECytraPanelLineStyle::Text, FText::Format(TileText(Type, TEXT("Desc")), Balance.MineYield, FText::AsNumber(Balance.MinePeriod), Balance.MineSlots));
	}
	else if (Type == ECytraTileType::Nest)
	{
		View.Title = TileText(Type, TEXT("Title"));
		AddLine(View, ECytraPanelLineStyle::Text, FText::Format(TileText(Type, TEXT("Desc")), Match.GetEggs()));
	}
	else
	{
		View.Title = TileText(Type, TEXT("Title"));
		AddLine(View, ECytraPanelLineStyle::Text, TileText(Type, TEXT("Desc")));
	}
	return View;
}

FText FCytraPanelView::RefusalText(const FCytraOrderCheck& Check)
{
	switch (Check.Refusal)
	{
	case ECytraOrderRefusal::None:
		return FText::GetEmpty();
	case ECytraOrderRefusal::NotAvailableHere:
		return CytraText::Get(TEXT("Refusal.NotAvailableHere"));
	case ECytraOrderRefusal::GameOver:
		return CytraText::Get(TEXT("Refusal.GameOver"));
	case ECytraOrderRefusal::JobInProgress:
		return CytraText::Get(TEXT("Refusal.JobInProgress"));
	case ECytraOrderRefusal::Unreachable:
		return CytraText::Get(TEXT("Refusal.Unreachable"));
	case ECytraOrderRefusal::MissingSpecialty:
		return FText::Format(CytraText::Get(TEXT("Refusal.MissingSpecialty")), Check.Spec ? RoleShort(Check.Spec->Role) : FText::GetEmpty());
	case ECytraOrderRefusal::NotEnoughGold:
		return FText::Format(CytraText::Get(TEXT("Refusal.NotEnoughGold")), Check.MissingGold);
	}
	return FText::GetEmpty();
}

FString FCytraPanelView::Signature() const
{
	FString Result = Title.ToString() + TEXT("|") + Tag.ToString();
	for (const FCytraPanelLine& Line : Lines)
	{
		Result += FString::Printf(TEXT("|L%d:%s:%s:%s"), static_cast<int32>(Line.Style), *Line.Text.ToString(),
			Line.Link ? *Line.Link->ToString() : TEXT(""), *Line.Suffix.ToString());
	}
	for (const FCytraPanelAction& Action : Actions)
	{
		Result += FString::Printf(TEXT("|A%s:%s:%s:%s:%s"), *Action.Action.Id.ToString(), *Action.Name.ToString(), *Action.Cost.ToString(),
			*Action.Meta.ToString(), *Action.Refusal.ToString());
	}
	return Result;
}
