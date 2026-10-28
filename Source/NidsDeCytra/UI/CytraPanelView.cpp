// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraPanelView.h"

#include "Rules/CytraMatch.h"
#include "Rules/CytraMath.h"
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

	FText TowerText(FName Tower, const TCHAR* Field)
	{
		return CytraText::Get(FString::Printf(TEXT("Tower.%s.%s"), *Tower.ToString(), Field));
	}

	/** A share such as 0.6 as a whole percentage. */
	int32 Percent(double Share)
	{
		return CytraMath::Round(Share * 100.0);
	}

	FText Decimal(double Value, int32 Digits)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = Digits;
		Options.MaximumFractionalDigits = Digits;
		return FText::AsNumber(Value, &Options);
	}

	void AddStat(FCytraPanelView& View, const TCHAR* LabelKey, const FText& Value)
	{
		FCytraPanelStat& Stat = View.Stats.AddDefaulted_GetRef();
		Stat.Label = CytraText::Get(LabelKey);
		Stat.Value = Value;
	}

	/**
	 * A button for the order if it applies to the cell, with its price (the cost unless Price is given),
	 * a short Extra description if any, the specialty and the job duration, and the reason it is refused.
	 */
	void AddAction(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, const FCytraOrderAction& Action,
		const FText& Name, const FText& Extra, const FText* Price = nullptr)
	{
		const FCytraOrderCheck Check = Match.CheckOrder(Cell, Action);
		if (!Check.Spec)
		{
			return;
		}
		FCytraPanelAction& Entry = View.Actions.AddDefaulted_GetRef();
		Entry.Action = Action;
		Entry.Name = Name;
		Entry.Cost = Price ? *Price : FText::Format(CytraText::Get(TEXT("Panel.Cost")), Check.Spec->Cost);
		Entry.Meta = Extra.IsEmpty()
			? FText::Format(CytraText::Get(TEXT("Panel.Meta")), RoleShort(Check.Spec->Role), FText::AsNumber(Check.Spec->Time))
			: FText::Format(CytraText::Get(TEXT("Panel.MetaWith")), Extra, RoleShort(Check.Spec->Role), FText::AsNumber(Check.Spec->Time));
		if (!Check.IsAccepted())
		{
			Entry.Refusal = FCytraPanelView::RefusalText(Check);
		}
	}

	void AddJobState(FCytraPanelView& View, const FCytraMatch& Match, const FCytraJob& Job, const FText& Ordered)
	{
		AddLine(View, ECytraPanelLineStyle::State, FText::Format(CytraText::Get(TEXT("Panel.JobState")), Ordered, JobLine(Match, Job)));
	}

	/** The job ordered on the cell, or a button for each work that applies there. */
	void AddWorks(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, ECytraTileType Type)
	{
		if (const FCytraJob* Job = Match.FindJobAt(Cell))
		{
			AddJobState(View, Match, *Job, WorkText(Job->Type, TEXT("Ordered")));
			return;
		}
		for (const FCytraWorkDef* Work : WorksOn(Match.GetBalance(), Type))
		{
			AddAction(View, Match, Cell, FCytraOrderAction::Work(Work->Id), WorkText(Work->Id, TEXT("Label")), FText::GetEmpty());
		}
	}

	/** A tower: its level and stats, then its upgrade and dismantling, or the job running on it. */
	void BuildTower(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, const FCytraTower& Tower, bool bHighSlot)
	{
		const FCytraBalance& Balance = Match.GetBalance();
		const FCytraTowerStats Stats = Balance.TowerStats(Tower.Type, Tower.Level, bHighSlot);
		View.Title = TowerText(Tower.Type, TEXT("Name"));
		View.TitleNote = FText::Format(CytraText::Get(TEXT("Panel.Level")), Tower.Level, Balance.Upgrade.MaxLevel);
		if (bHighSlot)
		{
			View.Tag = FText::Format(CytraText::Get(TEXT("Panel.HighTowerTag")), Percent(Balance.HighSlotRangeBonus - 1.0));
		}
		AddStat(View, TEXT("Panel.Damage"), FText::AsNumber(CytraMath::Round(Stats.Damage)));
		AddStat(View, TEXT("Panel.Range"), FText::Format(CytraText::Get(TEXT("Panel.RangeValue")), Decimal(Stats.Range, 1)));
		AddStat(View, TEXT("Panel.Rate"), FText::Format(CytraText::Get(TEXT("Panel.RateValue")), Decimal(Stats.Rate, 2)));
		AddStat(View, TEXT("Panel.Invested"), FText::Format(CytraText::Get(TEXT("Panel.Cost")), Tower.Invested));

		if (const FCytraJob* Job = Match.FindJobAt(Cell))
		{
			AddJobState(View, Match, *Job, CytraText::Get(Job->Kind == ECytraOrderKind::Dismantle ? TEXT("Panel.DismantleOrdered") : TEXT("Panel.UpgradeOrdered")));
			return;
		}
		if (Tower.Level >= Balance.Upgrade.MaxLevel)
		{
			AddLine(View, ECytraPanelLineStyle::Text, CytraText::Get(TEXT("Panel.MaxLevel")));
		}
		else
		{
			AddAction(View, Match, Cell, FCytraOrderAction::Upgrade(),
				FText::Format(CytraText::Get(TEXT("Panel.Upgrade")), Tower.Level + 1),
				FText::Format(CytraText::Get(TEXT("Panel.UpgradeExtra")), Percent(Balance.Upgrade.DamagePerLevel), Percent(Balance.Upgrade.RangePerLevel)));
		}
		const FText Refund = FText::Format(CytraText::Get(TEXT("Panel.Refund")), Balance.DismantleRefundOf(Tower.Invested));
		AddAction(View, Match, Cell, FCytraOrderAction::Dismantle(), CytraText::Get(TEXT("Panel.Dismantle")),
			FText::Format(CytraText::Get(TEXT("Panel.DismantleExtra")), Percent(Balance.DismantleRefund)), &Refund);
	}

	/** An open mine: the miners there, the income, the gold extracted so far, and whether a place is free. */
	void BuildOpenMine(FCytraPanelView& View, const FCytraMatch& Match, ECytraTileType Type)
	{
		const FCytraBalance& Balance = Match.GetBalance();
		const int32 Mining = Match.MinersMining();
		const int32 Taken = Match.MinersAtMine();
		View.Title = TileText(Type, TEXT("Title"));
		View.TitleNote = FText::Format(CytraText::Get(TEXT("Panel.MineMiners")), Mining, Balance.MineSlots);
		AddStat(View, TEXT("Panel.MineIncome"), FText::Format(CytraText::Get(TEXT("Panel.MineIncomeValue")), Mining * Balance.MineYield, FText::AsNumber(Balance.MinePeriod)));
		AddStat(View, TEXT("Panel.MineMined"), FText::Format(CytraText::Get(TEXT("Panel.Cost")), Match.GetWallet().Mined));

		const FText Places = Taken >= Balance.MineSlots ? FText::Format(CytraText::Get(TEXT("Panel.MineFull")), Balance.MineSlots)
			: Taken > 0 ? CytraText::Get(TEXT("Panel.MinePlaceLeft"))
			: CytraText::Get(TEXT("Panel.MineEmpty"));
		// A miner still on the way takes a place before he extracts.
		const TCHAR* State = Taken > Mining ? TEXT("Panel.MineStateComing") : TEXT("Panel.MineState");
		AddLine(View, ECytraPanelLineStyle::Text, FText::Format(CytraText::Get(State), Places));
	}

	void BuildSlot(FCytraPanelView& View, const FCytraMatch& Match, const FIntPoint& Cell, ECytraTileType Type)
	{
		const FCytraGrid& Grid = Match.GetGrid();
		const bool bHighSlot = Type == ECytraTileType::HighSlot;
		if (const FCytraTower* Tower = Grid.GetTower(Cell))
		{
			BuildTower(View, Match, Cell, *Tower, bHighSlot);
			return;
		}

		View.Title = TileText(Type, TEXT("Title"));
		if (bHighSlot)
		{
			View.Tag = FText::Format(CytraText::Get(TEXT("Panel.HighTag")), Percent(Match.GetBalance().HighSlotRangeBonus - 1.0));
		}
		if (const FCytraJob* Job = Match.FindJobAt(Cell))
		{
			AddJobState(View, Match, *Job, TowerText(Job->Type, TEXT("Ordered")));
			return;
		}
		if (Grid.IsReachable(Cell))
		{
			for (const TPair<FName, FCytraTowerDef>& Tower : Match.GetBalance().Towers)
			{
				AddAction(View, Match, Cell, FCytraOrderAction::Build(Tower.Key), TowerText(Tower.Key, TEXT("Name")), TowerText(Tower.Key, TEXT("Desc")));
			}
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
	else if (Type == ECytraTileType::Mine && bOpened)
	{
		BuildOpenMine(View, Match, Type);
	}
	else if (Type == ECytraTileType::Mine)
	{
		const FCytraBalance& Balance = Match.GetBalance();
		View.Title = TileText(Type, TEXT("Title"));
		AddLine(View, ECytraPanelLineStyle::Text, FText::Format(TileText(Type, TEXT("Desc")), Balance.MineYield, FText::AsNumber(Balance.MinePeriod), Balance.MineSlots));
		AddWorks(View, Match, *Cell, Type);
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

FCytraPanelView FCytraPanelView::BuildCrew(const FCytraMatch& Match)
{
	const FCytraBalance& Balance = Match.GetBalance();
	FCytraPanelView View;
	View.Title = CytraText::Get(TEXT("Crew.Title"));
	View.TitleNote = FText::Format(CytraText::Get(TEXT("Crew.Count")), Match.GetWorkers().Num(), Balance.MaxCrew);
	for (const TPair<FName, FCytraRoleDef>& Role : Balance.Roles)
	{
		const FCytraRecruitCheck Check = Match.CheckRecruit(Role.Key);
		FCytraPanelAction& Entry = View.Actions.AddDefaulted_GetRef();
		Entry.Recruit = Role.Key;
		Entry.Name = FText::Format(CytraText::Get(TEXT("Crew.Recruit")), RoleShort(Role.Key));
		Entry.Cost = FText::Format(CytraText::Get(TEXT("Panel.Cost")), Role.Value.RecruitCost);
		Entry.Meta = FText::Format(CytraText::Get(TEXT("Crew.Spec")), CytraText::Get(FString::Printf(TEXT("Role.%s.Spec"), *Role.Key.ToString())));
		Entry.Refusal = RecruitRefusalText(Check);
	}
	return View;
}

FText FCytraPanelView::RecruitRefusalText(const FCytraRecruitCheck& Check)
{
	switch (Check.Refusal)
	{
	case ECytraRecruitRefusal::None:
		return FText::GetEmpty();
	case ECytraRecruitRefusal::UnknownRole:
		return CytraText::Get(TEXT("Refusal.NotAvailableHere"));
	case ECytraRecruitRefusal::CrewFull:
		return FText::Format(CytraText::Get(TEXT("Refusal.CrewFull")), Check.MaxCrew);
	case ECytraRecruitRefusal::NotEnoughGold:
		return FText::Format(CytraText::Get(TEXT("Refusal.NotEnoughGold")), Check.MissingGold);
	}
	return FText::GetEmpty();
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
	FString Result = Title.ToString() + TEXT("|") + TitleNote.ToString() + TEXT("|") + Tag.ToString();
	for (const FCytraPanelStat& Stat : Stats)
	{
		Result += FString::Printf(TEXT("|S%s:%s"), *Stat.Label.ToString(), *Stat.Value.ToString());
	}
	for (const FCytraPanelLine& Line : Lines)
	{
		Result += FString::Printf(TEXT("|L%d:%s:%s:%s"), static_cast<int32>(Line.Style), *Line.Text.ToString(),
			Line.Link ? *Line.Link->ToString() : TEXT(""), *Line.Suffix.ToString());
	}
	for (const FCytraPanelAction& Action : Actions)
	{
		Result += FString::Printf(TEXT("|A%s%s:%s:%s:%s:%s"), *Action.Recruit.ToString(), *Action.Action.Id.ToString(), *Action.Name.ToString(), *Action.Cost.ToString(),
			*Action.Meta.ToString(), *Action.Refusal.ToString());
	}
	return Result;
}
