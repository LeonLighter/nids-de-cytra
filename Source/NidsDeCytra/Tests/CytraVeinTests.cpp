// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"
#include "UI/CytraMessages.h"
#include "UI/CytraPanelView.h"

namespace CytraVeinTest
{
	constexpr double Dt = 1.0 / 60.0;

	const FName Miner(TEXT("Miner"));
	const FIntPoint Mine(14, 11);

	/** The prototype balance, with veins and extraction levels set as given (a Reserve of 0 leaves the mine endless). */
	struct FSetup
	{
		int32 Reserve = 0;
		int32 DeepenCost = 120;
		bool bExtraction = false;
		TArray<FName> StartRoles = { Miner };
	};

	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test, const FSetup& Setup)
	{
		FCytraMapDef Map;
		FCytraBalance Balance;
		FCytraWaveSchedule Waves;
		if (!CytraTest::LoadAll(Test, Map, Balance, Waves))
		{
			return {};
		}
		Balance.StartRoles = Setup.StartRoles;
		if (Setup.Reserve > 0)
		{
			Balance.MineVein = FCytraMineVeinDef{ true, Miner, Setup.Reserve, 1.6, Setup.DeepenCost, 1.7, 10.0 };
		}
		if (Setup.bExtraction)
		{
			Balance.MineExtraction = FCytraMineExtractionDef{ true, Miner, 75, 1.7, 8.0, 1.25 };
		}
		return FCytraMatch(Map, Balance, Waves);
	}

	void StepFor(FCytraMatch& Match, double Seconds)
	{
		for (int32 i = 0; i < FMath::RoundToInt32(Seconds / Dt); ++i)
		{
			Match.Step(Dt);
		}
	}

	bool StepUntilIdle(FCytraMatch& Match)
	{
		for (int32 i = 0; i < 60 * 60 && !Match.GetJobs().IsEmpty(); ++i)
		{
			Match.Step(Dt);
		}
		return Match.GetJobs().IsEmpty();
	}

	bool OpenMine(FAutomationTestBase& Test, FCytraMatch& Match)
	{
		return Test.TestTrue(TEXT("Open mine accepted"), Match.Order(Mine, FCytraOrderAction::Work(TEXT("OpenMine"))).IsAccepted())
			&& Test.TestTrue(TEXT("Mine opened"), StepUntilIdle(Match));
	}

	const FCytraMineState& MineState(const FCytraMatch& Match)
	{
		return *Match.GetGrid().GetMine(Mine);
	}

	bool HasEvent(const TArray<FCytraMatchEvent>& Events, ECytraMatchEventType Type)
	{
		return Events.ContainsByPredicate([Type](const FCytraMatchEvent& Event) { return Event.Type == Type; });
	}

	FString StatText(const FCytraPanelView& View, int32 Index)
	{
		return View.Stats.IsValidIndex(Index) ? View.Stats[Index].Label.ToString() + TEXT(" ") + View.Stats[Index].Value.ToString() : FString(TEXT("<no stat>"));
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinValuesTest, "Cytra.Rules.Mine.VeinValues", CytraTest::Flags)

bool FCytraVeinValuesTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Live;
	FCytraBalance Reference;
	if (!CytraTest::LoadMapAndLiveBalance(*this, Map, Live) || !CytraTest::LoadMapAndBalance(*this, Map, Reference))
	{
		return false;
	}

	// The game: Base x Factor^(n-1), rounded.
	TestEqual(TEXT("Opening above the starting gold"), Live.Works.FindChecked(TEXT("OpenMine")).Cost, 150);
	TestEqual(TEXT("Vein 1"), Live.VeinReserve(1).Get(0), 150);
	TestEqual(TEXT("Vein 2"), Live.VeinReserve(2).Get(0), 240);
	TestEqual(TEXT("Vein 3"), Live.VeinReserve(3).Get(0), 384);
	TestEqual(TEXT("Dig to vein 2"), Live.DeepenCost(1), 120);
	TestEqual(TEXT("Dig to vein 3"), Live.DeepenCost(2), 204);
	TestEqual(TEXT("Dig to vein 4"), Live.DeepenCost(3), 347);
	TestEqual(TEXT("Dig time"), Live.MineVein.Time, 10.0);
	TestEqual(TEXT("Extraction 2"), Live.ExtractionCost(1), 75);
	TestEqual(TEXT("Extraction 3"), Live.ExtractionCost(2), 128);
	TestEqual(TEXT("Extraction 4"), Live.ExtractionCost(3), 217);
	TestEqual(TEXT("Extraction time"), Live.MineExtraction.Time, 8.0);
	TestEqual(TEXT("Yield 1"), Live.MineYieldAt(1), 2.0);
	TestEqual(TEXT("Yield 2"), Live.MineYieldAt(2), 2.5);
	TestEqual(TEXT("Yield 3"), Live.MineYieldAt(3), 3.125);
	TestEqual(TEXT("No maximum: very deep veins stay within int32"), Live.DeepenCost(100), MAX_int32);

	// The prototype: an endless mine with a fixed yield.
	TestFalse(TEXT("Prototype mine endless"), Reference.VeinReserve(1).IsSet());
	TestEqual(TEXT("Prototype yield fixed"), Reference.MineYieldAt(5), 2.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinExhaustsTest, "Cytra.Rules.Mine.VeinExhausts", CytraTest::Flags)

bool FCytraVeinExhaustsTest::RunTest(const FString& Parameters)
{
	FSetup Setup;
	Setup.Reserve = 5;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Setup);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	TestEqual(TEXT("First vein"), MineState(*Match).Vein, 1);
	TestEqual(TEXT("Reserve of the first vein"), MineState(*Match).Reserve.Get(-1), 5);
	TestEqual(TEXT("The miner goes down"), Match->MinersMining(), 1);
	Match->ConsumeEvents();

	StepFor(*Match, 6.1);
	TestEqual(TEXT("2 gold at 6 s"), Match->GetWallet().Mined, 2);
	TestEqual(TEXT("Taken from the reserve"), MineState(*Match).Reserve.Get(-1), 3);
	StepFor(*Match, 6.0);
	TestEqual(TEXT("4 gold at 12 s"), Match->GetWallet().Mined, 4);

	// The last period pays only what is left, then everyone leaves the mine.
	Match->ConsumeEvents();
	StepFor(*Match, 6.0);
	TestEqual(TEXT("Only the last gold"), Match->GetWallet().Mined, 5);
	TestTrue(TEXT("Exhausted"), MineState(*Match).IsExhausted());
	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	TestTrue(TEXT("Exhaustion event"), HasEvent(Events, ECytraMatchEventType::MineExhausted));
	const FCytraMatchEvent* Yield = Events.FindByPredicate([](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::MineYield; });
	if (TestNotNull(TEXT("Yield event"), Yield))
	{
		TestEqual(TEXT("Yield cut by the reserve"), Yield->Gold, 1);
	}
	TestEqual(TEXT("Nobody at the mine"), Match->MinersAtMine(), 0);
	TestNull(TEXT("No mine to work"), Match->FindWorkableMine());

	StepFor(*Match, 30.0);
	TestEqual(TEXT("Nothing more"), Match->GetWallet().Mined, 5);
	TestEqual(TEXT("Still nobody at the mine"), Match->MinersAtMine(), 0);
	const FCytraWorker& Worker = Match->GetWorkers()[0];
	TestTrue(TEXT("The miner waits"), Worker.State == ECytraWorkerState::Idle);
	TestTrue(TEXT("At the main camp"), Worker.Position == FVector2d(Match->GetMap().Camp.X, Match->GetMap().Camp.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinDeepenTest, "Cytra.Rules.Mine.Deepen", CytraTest::Flags)

bool FCytraVeinDeepenTest::RunTest(const FString& Parameters)
{
	// Digging before the vein runs out: the gold left adds to the next vein.
	FSetup Setup;
	Setup.Reserve = 5;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Setup);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	const int32 GoldBefore = Match->GetWallet().Gold;
	const FCytraOrderResult Dig = Match->Order(Mine, FCytraOrderAction::DeepenMine());
	if (!TestTrue(TEXT("Dig accepted"), Dig.IsAccepted()))
	{
		return false;
	}
	TestEqual(TEXT("Dig cost"), Dig.Check.Spec->Cost, 120);
	TestEqual(TEXT("Dig time"), Dig.Check.Spec->Time, 10.0);
	TestEqual(TEXT("Dig paid"), Match->GetWallet().Gold, GoldBefore - 120);
	TestEqual(TEXT("The miner leaves the mine to dig"), Match->MinersAtMine(), 0);
	Match->ConsumeEvents();
	if (!TestTrue(TEXT("Dig done"), StepUntilIdle(*Match)))
	{
		return false;
	}
	TestEqual(TEXT("Second vein"), MineState(*Match).Vein, 2);
	TestEqual(TEXT("5 left + 8 in vein 2"), MineState(*Match).Reserve.Get(-1), 13);
	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	const FCytraMatchEvent* Done = Events.FindByPredicate([](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::JobDone; });
	if (TestNotNull(TEXT("Dig event"), Done))
	{
		TestTrue(TEXT("Dig job"), Done->Job == ECytraOrderKind::DeepenMine);
		TestEqual(TEXT("Vein reached"), Done->Level, 2);
		TestEqual(TEXT("Reserve added"), Done->Gold, 8);
		TestEqual(TEXT("Message"), FCytraMessages::FromEvent(*Done)->Text.ToString(), FString(TEXT("Filon 2 atteint : +8 or de réserve")));
	}
	TestEqual(TEXT("Back to the mine"), Match->MinersMining(), 1);

	// An exhausted mine dug to its next vein: the miners go back down.
	StepFor(*Match, 6.0 * 7 + 0.1);
	TestTrue(TEXT("Exhausted after 13 gold"), MineState(*Match).IsExhausted());
	TestEqual(TEXT("13 gold mined"), Match->GetWallet().Mined, 13);
	TestEqual(TEXT("Dig to vein 3 costs more"), Match->CheckOrder(Mine, FCytraOrderAction::DeepenMine()).Spec->Cost, 204);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinRefillTest, "Cytra.Rules.Mine.Refill", CytraTest::Flags)

bool FCytraVeinRefillTest::RunTest(const FString& Parameters)
{
	FSetup Setup;
	Setup.Reserve = 2;
	Setup.DeepenCost = 50;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Setup);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	StepFor(*Match, 6.1);
	if (!TestTrue(TEXT("Exhausted"), MineState(*Match).IsExhausted()))
	{
		return false;
	}
	StepFor(*Match, 20.0);
	TestTrue(TEXT("Accepted on an exhausted mine"), Match->Order(Mine, FCytraOrderAction::DeepenMine()).IsAccepted());
	if (!TestTrue(TEXT("Dig done"), StepUntilIdle(*Match)))
	{
		return false;
	}
	TestEqual(TEXT("Vein 2 only"), MineState(*Match).Reserve.Get(-1), 3);
	TestNotNull(TEXT("Workable again"), Match->FindWorkableMine());
	TestEqual(TEXT("The miner goes back down"), Match->MinersMining(), 1);
	StepFor(*Match, 6.1);
	TestEqual(TEXT("Extraction resumes"), Match->GetWallet().Mined, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinImproveTest, "Cytra.Rules.Mine.Improve", CytraTest::Flags)

bool FCytraVeinImproveTest::RunTest(const FString& Parameters)
{
	FSetup Setup;
	Setup.bExtraction = true;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Setup);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	TestFalse(TEXT("Endless without veins"), MineState(*Match).Reserve.IsSet());
	const FCytraOrderResult Improve = Match->Order(Mine, FCytraOrderAction::ImproveMine());
	if (!TestTrue(TEXT("Improvement accepted"), Improve.IsAccepted()))
	{
		return false;
	}
	TestEqual(TEXT("Improvement cost"), Improve.Check.Spec->Cost, 75);
	TestEqual(TEXT("Improvement time"), Improve.Check.Spec->Time, 8.0);
	TestEqual(TEXT("Message"), FCytraMessages::FromOrder(Improve).Text.ToString(), FString(TEXT("Extraction : −75 or")));
	if (!TestTrue(TEXT("Improvement done"), StepUntilIdle(*Match)))
	{
		return false;
	}
	TestEqual(TEXT("Level 2"), MineState(*Match).Extraction, 2);
	TestEqual(TEXT("Vein unchanged"), MineState(*Match).Vein, 1);
	TestEqual(TEXT("Next improvement costs more"), Match->CheckOrder(Mine, FCytraOrderAction::ImproveMine()).Spec->Cost, 128);
	TestTrue(TEXT("Digging stays unavailable"), Match->CheckOrder(Mine, FCytraOrderAction::DeepenMine()).Refusal == ECytraOrderRefusal::NotAvailableHere);

	// 2.5 gold per period: the half carries over, 2 then 3.
	const int32 Before = Match->GetWallet().Mined;
	Match->ConsumeEvents();
	StepFor(*Match, 6.0 * 4 + 0.1);
	TestEqual(TEXT("10 gold in 4 periods"), Match->GetWallet().Mined - Before, 10);
	TArray<int32> Yields;
	for (const FCytraMatchEvent& Event : Match->ConsumeEvents())
	{
		if (Event.Type == ECytraMatchEventType::MineYield)
		{
			Yields.Add(Event.Gold);
		}
	}
	TestEqual(TEXT("Yields"), Yields, TArray<int32>{ 2, 3, 2, 3 });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinRefusalsTest, "Cytra.Rules.Mine.LevelRefusals", CytraTest::Flags)

bool FCytraVeinRefusalsTest::RunTest(const FString& Parameters)
{
	// The prototype has neither order.
	FSetup Prototype;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Prototype);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	TestTrue(TEXT("No vein in the prototype"), Match->CheckOrder(Mine, FCytraOrderAction::DeepenMine()).Refusal == ECytraOrderRefusal::NotAvailableHere);
	TestTrue(TEXT("No extraction level in the prototype"), Match->CheckOrder(Mine, FCytraOrderAction::ImproveMine()).Refusal == ECytraOrderRefusal::NotAvailableHere);

	FSetup Setup;
	Setup.Reserve = 150;
	Setup.DeepenCost = 500;
	Setup.bExtraction = true;
	Match = MakeMatch(*this, Setup);
	if (!Match)
	{
		return false;
	}
	TestTrue(TEXT("Closed mine"), Match->CheckOrder(Mine, FCytraOrderAction::DeepenMine()).Refusal == ECytraOrderRefusal::NotAvailableHere);
	TestTrue(TEXT("Not a mine"), Match->CheckOrder(FIntPoint(13, 8), FCytraOrderAction::ImproveMine()).Refusal == ECytraOrderRefusal::NotAvailableHere);
	if (!OpenMine(*this, *Match))
	{
		return false;
	}
	const FCytraOrderCheck Poor = Match->CheckOrder(Mine, FCytraOrderAction::DeepenMine());
	TestTrue(TEXT("Not enough gold"), Poor.Refusal == ECytraOrderRefusal::NotEnoughGold);
	TestEqual(TEXT("Missing gold"), Poor.MissingGold, 500 - Match->GetWallet().Gold);

	TestTrue(TEXT("Improvement accepted"), Match->Order(Mine, FCytraOrderAction::ImproveMine()).IsAccepted());
	TestTrue(TEXT("One job at a time on the mine"), Match->CheckOrder(Mine, FCytraOrderAction::ImproveMine()).Refusal == ECytraOrderRefusal::JobInProgress);
	StepUntilIdle(*Match);

	const int32 MinerId = Match->GetWorkers()[0].Id;
	Match->Dismiss(MinerId);
	TestTrue(TEXT("No miner"), Match->CheckOrder(Mine, FCytraOrderAction::ImproveMine()).Refusal == ECytraOrderRefusal::MissingSpecialty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraVeinPanelTest, "Cytra.UI.Panel.MineVeins", CytraTest::Flags)

bool FCytraVeinPanelTest::RunTest(const FString& Parameters)
{
	FSetup Setup;
	Setup.Reserve = 2;
	Setup.bExtraction = true;
	TOptional<FCytraMatch> Match = MakeMatch(*this, Setup);
	if (!Match)
	{
		return false;
	}
	const FCytraPanelView Closed = FCytraPanelView::Build(*Match, Mine);
	if (TestEqual(TEXT("Closed lines"), Closed.Lines.Num(), 2))
	{
		TestEqual(TEXT("First vein"), Closed.Lines[1].Text.ToString(), FString(TEXT("Le premier filon contient 2 or. Creuser plus loin coûte de l’or.")));
	}
	if (!OpenMine(*this, *Match))
	{
		return false;
	}

	const FCytraPanelView Open = FCytraPanelView::Build(*Match, Mine);
	TestEqual(TEXT("Income"), StatText(Open, 0), FString(TEXT("Revenu 2 or toutes les 6 s")));
	TestEqual(TEXT("Vein"), StatText(Open, 2), FString(TEXT("Filon 1")));
	TestEqual(TEXT("Reserve"), StatText(Open, 3), FString(TEXT("Réserve 2 or")));
	TestEqual(TEXT("Extraction"), StatText(Open, 4), FString(TEXT("Extraction niveau 1")));
	if (TestEqual(TEXT("Two orders"), Open.Actions.Num(), 2))
	{
		TestEqual(TEXT("Dig name"), Open.Actions[0].Name.ToString(), FString(TEXT("Creuser jusqu’au filon 2")));
		TestEqual(TEXT("Dig cost"), Open.Actions[0].Cost.ToString(), FString(TEXT("120 or")));
		TestEqual(TEXT("Dig details"), Open.Actions[0].Meta.ToString(), FString(TEXT("+3 or de réserve · mineur · 10 s de chantier")));
		TestEqual(TEXT("Improve name"), Open.Actions[1].Name.ToString(), FString(TEXT("Améliorer l’extraction au niveau 2")));
		TestEqual(TEXT("Improve cost"), Open.Actions[1].Cost.ToString(), FString(TEXT("75 or")));
		TestEqual(TEXT("Improve details"), Open.Actions[1].Meta.ToString(),
			FString::Printf(TEXT("%s or toutes les 6 s par mineur · mineur · 8 s de chantier"), *FText::AsNumber(2.5).ToString()));
	}

	// A job on the mine replaces its orders.
	Match->Order(Mine, FCytraOrderAction::ImproveMine());
	const FCytraPanelView Busy = FCytraPanelView::Build(*Match, Mine);
	TestTrue(TEXT("No order while improving"), Busy.Actions.IsEmpty());
	TestTrue(TEXT("Job state first"), Busy.Lines.Num() > 0 && Busy.Lines[0].Style == ECytraPanelLineStyle::State);
	StepUntilIdle(*Match);

	// Exhausted.
	StepFor(*Match, 6.1);
	const FCytraPanelView Empty = FCytraPanelView::Build(*Match, Mine);
	TestEqual(TEXT("Reserve empty"), StatText(Empty, 3), FString(TEXT("Réserve épuisée")));
	if (TestEqual(TEXT("One line"), Empty.Lines.Num(), 1))
	{
		TestTrue(TEXT("Warning"), Empty.Lines[0].Style == ECytraPanelLineStyle::Warning);
		TestEqual(TEXT("Exhausted"), Empty.Lines[0].Text.ToString(),
			FString(TEXT("Filon épuisé : les mineurs remontent. Creuse jusqu’au filon suivant pour reprendre l’extraction.")));
	}
	TestEqual(TEXT("Both orders still offered"), Empty.Actions.Num(), 2);
	return true;
}

}

#endif
