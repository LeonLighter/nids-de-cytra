// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"
#include "UI/CytraPanelView.h"

namespace CytraPanelTest
{
	constexpr double Dt = 1.0 / 60.0;

	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test, const TArray<FName>* StartRoles = nullptr)
	{
		FCytraMapDef Map;
		FCytraBalance Balance;
		FCytraWaveSchedule Waves;
		if (!CytraTest::LoadAll(Test, Map, Balance, Waves))
		{
			return {};
		}
		if (StartRoles)
		{
			Balance.StartRoles = *StartRoles;
		}
		return FCytraMatch(Map, Balance, Waves);
	}

	FCytraPanelView Panel(const FCytraMatch& Match, const FIntPoint& Cell)
	{
		return FCytraPanelView::Build(Match, Cell);
	}

	FString LineText(const FCytraPanelView& View, int32 Index)
	{
		return View.Lines.IsValidIndex(Index) ? View.Lines[Index].Text.ToString() : FString(TEXT("<no line>"));
	}

	FString Refusal(const FCytraMatch& Match, const FIntPoint& Cell)
	{
		const FCytraPanelView View = Panel(Match, Cell);
		return View.Actions.Num() == 1 ? View.Actions[0].Refusal.ToString() : FString(TEXT("<no single action>"));
	}

	FCytraOrderAction Work(const TCHAR* Id)
	{
		return FCytraOrderAction::Work(FName(Id));
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelHelpTest, "Cytra.UI.Panel.Help", CytraTest::Flags)

bool FCytraPanelHelpTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	const FCytraPanelView View = FCytraPanelView::Build(*Match, {});
	TestEqual(TEXT("Title"), View.Title.ToString(), FString(TEXT("Ordres")));
	TestEqual(TEXT("Hint"), LineText(View, 0), FString(TEXT("Clique une case de la carte pour donner un ordre.")));
	if (TestEqual(TEXT("Hint and four steps"), View.Lines.Num(), 5))
	{
		TestTrue(TEXT("Steps"), View.Lines[4].Style == ECytraPanelLineStyle::Step);
		TestEqual(TEXT("Last step"), LineText(View, 4), FString(TEXT("Lance la vague quand tes ouvriers sont en place.")));
	}
	TestTrue(TEXT("No action"), View.Actions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelAccessChainTest, "Cytra.UI.Panel.AccessChain", CytraTest::Flags)

bool FCytraPanelAccessChainTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraPanelView View = Panel(*Match, FIntPoint(15, 7));
	TestEqual(TEXT("Title"), View.Title.ToString(), FString(TEXT("Promontoire")));
	TestEqual(TEXT("Tag"), View.Tag.ToString(), FString(TEXT("Portée +30 %")));
	if (!TestEqual(TEXT("Warning and two steps"), View.Lines.Num(), 3))
	{
		return false;
	}
	TestTrue(TEXT("Warning"), View.Lines[0].Style == ECytraPanelLineStyle::Warning);
	TestEqual(TEXT("Warning text"), LineText(View, 0), FString(TEXT("Coupé du camp. Accès à ouvrir, dans l’ordre :")));
	TestEqual(TEXT("First step"), LineText(View, 1), FString(TEXT("Déblayer l’éboulis")));
	TestEqual(TEXT("First step links the rubble"), View.Lines[1].Link, TOptional<FIntPoint>(FIntPoint(13, 8)));
	TestEqual(TEXT("First step details"), View.Lines[1].Suffix.ToString(), FString(TEXT("(mineur, 30 or)")));
	TestEqual(TEXT("Second step"), LineText(View, 2), FString(TEXT("Poser une échelle")));
	TestEqual(TEXT("Second step links the cliff"), View.Lines[2].Link, TOptional<FIntPoint>(FIntPoint(14, 7)));
	TestEqual(TEXT("Second step details"), View.Lines[2].Suffix.ToString(), FString(TEXT("(charpentier, 45 or)")));
	TestTrue(TEXT("No tower order on a slot cut off from the camp"), View.Actions.IsEmpty());

	const FCytraPanelView Reachable = Panel(*Match, FIntPoint(19, 6));
	TestEqual(TEXT("Reachable slot title"), Reachable.Title.ToString(), FString(TEXT("Emplacement")));
	TestTrue(TEXT("Reachable slot has no chain"), Reachable.Lines.IsEmpty());
	TestEqual(TEXT("Reachable slot takes towers"), Reachable.Actions.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelJobLifeTest, "Cytra.UI.Panel.JobLife", CytraTest::Flags)

bool FCytraPanelJobLifeTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	const FIntPoint Gap(9, 6);

	const FCytraPanelView Before = Panel(*Match, Gap);
	TestEqual(TEXT("Title"), Before.Title.ToString(), FString(TEXT("Gouffre")));
	TestEqual(TEXT("Description"), LineText(Before, 0), FString(TEXT("Une faille coupe la corniche. Un charpentier peut y jeter un pont.")));
	if (!TestEqual(TEXT("One order"), Before.Actions.Num(), 1))
	{
		return false;
	}
	const FCytraPanelAction& Bridge = Before.Actions[0];
	TestEqual(TEXT("Order name"), Bridge.Name.ToString(), FString(TEXT("Jeter un pont")));
	TestEqual(TEXT("Order cost"), Bridge.Cost.ToString(), FString(TEXT("60 or")));
	TestEqual(TEXT("Order details"), Bridge.Meta.ToString(), FString(TEXT("charpentier · 9 s de chantier")));
	TestTrue(TEXT("Order accepted"), Bridge.IsEnabled());

	Match->Order(Gap, Bridge.Action);
	const FCytraPanelView Ordered = Panel(*Match, Gap);
	TestTrue(TEXT("No order while the job runs"), Ordered.Actions.IsEmpty());
	TestEqual(TEXT("Worker on the way"), LineText(Ordered, 1), FString(TEXT("Jeter un pont : commandé. Un charpentier est en route.")));
	TestTrue(TEXT("State style"), Ordered.Lines.Last().Style == ECytraPanelLineStyle::State);
	TestNotEqual(TEXT("Signature changes"), Ordered.Signature(), Before.Signature());

	for (int32 i = 0; i < 300; ++i)
	{
		Match->Step(Dt);
	}
	TestEqual(TEXT("Worker on site"), LineText(Panel(*Match, Gap), 1), FString(TEXT("Jeter un pont : commandé. Chantier en cours.")));

	for (int32 i = 0; i < 600; ++i)
	{
		Match->Step(Dt);
	}
	const FCytraPanelView Done = Panel(*Match, Gap);
	TestEqual(TEXT("Opened title"), Done.Title.ToString(), FString(TEXT("Pont")));
	TestEqual(TEXT("Opened description"), LineText(Done, 0), FString(TEXT("L’accès est ouvert. Les ouvriers peuvent passer.")));
	TestTrue(TEXT("Nothing more to order"), Done.Actions.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelRefusalsTest, "Cytra.UI.Panel.Refusals", CytraTest::Flags)

bool FCytraPanelRefusalsTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	TestEqual(TEXT("Unreachable"), Refusal(*Match, FIntPoint(14, 7)), FString(TEXT("Inaccessible : ouvre d’abord un accès")));

	Match->Order(FIntPoint(4, 7), Work(TEXT("Outpost")));
	Match->Order(FIntPoint(9, 6), Work(TEXT("Bridge")));
	TestEqual(TEXT("Gold left"), Match->GetWallet().Gold, 40);
	TestEqual(TEXT("Not enough gold"), Refusal(*Match, FIntPoint(7, 3)), FString(TEXT("Il manque 5 or")));
	TestEqual(TEXT("Waiting job"), LineText(Panel(*Match, FIntPoint(9, 6)), 1), FString(TEXT("Jeter un pont : commandé. En attente d’un charpentier libre.")));
	TestEqual(TEXT("Outpost ordered"), LineText(Panel(*Match, FIntPoint(4, 7)), 1), FString(TEXT("Avant-poste commandé. Un charpentier est en route.")));

	const TArray<FName> CarpenterOnly = { FName(TEXT("Carpenter")) };
	TOptional<FCytraMatch> NoMiner = MakeMatch(*this, &CarpenterOnly);
	if (NoMiner)
	{
		TestEqual(TEXT("Missing specialty"), Refusal(*NoMiner, FIntPoint(13, 8)), FString(TEXT("Il faut un mineur dans l’équipe")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelTilesTest, "Cytra.UI.Panel.Tiles", CytraTest::Flags)

bool FCytraPanelTilesTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraPanelView Nest = Panel(*Match, FIntPoint(22, 7));
	TestEqual(TEXT("Nest"), Nest.Title.ToString(), FString(TEXT("Nid de Cytra")));
	TestEqual(TEXT("Eggs left"), LineText(Nest, 0), FString(TEXT("12 œufs à garder. Chaque assaillant qui arrive ici en détruit.")));
	TestEqual(TEXT("Camp"), Panel(*Match, FIntPoint(20, 9)).Title.ToString(), FString(TEXT("Camp du Conclave")));
	TestEqual(TEXT("Road"), Panel(*Match, FIntPoint(10, 5)).Title.ToString(), FString(TEXT("Chemin des assaillants")));
	TestEqual(TEXT("Ledge"), Panel(*Match, FIntPoint(5, 6)).Title.ToString(), FString(TEXT("Galerie")));
	TestEqual(TEXT("Rock"), Panel(*Match, FIntPoint(0, 0)).Title.ToString(), FString(TEXT("Roche pleine")));

	const FCytraPanelView Mine = Panel(*Match, FIntPoint(14, 11));
	TestEqual(TEXT("Mine"), Mine.Title.ToString(), FString(TEXT("Mine d’or")));
	TestEqual(TEXT("Mine description"), LineText(Mine, 0),
		FString(TEXT("Une galerie condamnée. Une fois rouverte, chaque mineur sans chantier y extrait 2 or toutes les 6 secondes. 2 places.")));
	TestEqual(TEXT("Mine can be opened"), Mine.Actions.Num(), 1);

	const FCytraPanelView Outpost = Panel(*Match, FIntPoint(4, 7));
	TestEqual(TEXT("Outpost site"), Outpost.Title.ToString(), FString(TEXT("Site d’avant-poste")));
	if (TestEqual(TEXT("Outpost order"), Outpost.Actions.Num(), 1))
	{
		TestEqual(TEXT("Outpost order name"), Outpost.Actions[0].Name.ToString(), FString(TEXT("Monter l’avant-poste")));
		TestEqual(TEXT("Outpost order details"), Outpost.Actions[0].Meta.ToString(), FString(TEXT("charpentier · 10 s de chantier")));
	}
	return true;
}

	/** Steps until no job is left. */
	void StepUntilIdle(FCytraMatch& Match)
	{
		for (int32 i = 0; i < 60 * 60 && !Match.GetJobs().IsEmpty(); ++i)
		{
			Match.Step(Dt);
		}
	}

	FString StatText(const FCytraPanelView& View, int32 Index)
	{
		return View.Stats.IsValidIndex(Index) ? View.Stats[Index].Label.ToString() + TEXT(" ") + View.Stats[Index].Value.ToString() : FString(TEXT("<no stat>"));
	}

	/** A decimal as the panel writes it in the current culture. */
	FString Decimal(double Value, int32 Digits)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = Digits;
		Options.MaximumFractionalDigits = Digits;
		return FText::AsNumber(Value, &Options).ToString();
	}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelTowerOrdersTest, "Cytra.UI.Panel.TowerOrders", CytraTest::Flags)

bool FCytraPanelTowerOrdersTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	const FIntPoint Slot(3, 2);

	// An empty slot offers the three towers, in data order.
	const FCytraPanelView Empty = Panel(*Match, Slot);
	TestEqual(TEXT("Slot title"), Empty.Title.ToString(), FString(TEXT("Emplacement")));
	if (!TestEqual(TEXT("Three towers"), Empty.Actions.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Ballista name"), Empty.Actions[0].Name.ToString(), FString(TEXT("Baliste")));
	TestEqual(TEXT("Ballista cost"), Empty.Actions[0].Cost.ToString(), FString(TEXT("50 or")));
	TestEqual(TEXT("Ballista details"), Empty.Actions[0].Meta.ToString(), FString(TEXT("Tir rapide sur une cible · charpentier · 5 s de chantier")));
	TestTrue(TEXT("Ballista accepted"), Empty.Actions[0].IsEnabled());
	TestEqual(TEXT("Mortar details"), Empty.Actions[1].Meta.ToString(), FString(TEXT("Obus lent, dégâts de zone · charpentier · 8 s de chantier")));
	TestEqual(TEXT("Coil name"), Empty.Actions[2].Name.ToString(), FString(TEXT("Bobine gnome")));
	TestEqual(TEXT("Coil refused"), Empty.Actions[2].Refusal.ToString(), FString(TEXT("Il faut un ingénieur dans l’équipe")));

	// While it is built, the slot shows the job.
	Match->Order(Slot, Empty.Actions[0].Action);
	const FCytraPanelView Building = Panel(*Match, Slot);
	TestEqual(TEXT("Build ordered"), LineText(Building, 0), FString(TEXT("Baliste commandée. Un charpentier est en route.")));
	TestTrue(TEXT("No order while building"), Building.Actions.IsEmpty());

	// Then the tower, its stats and its orders.
	StepUntilIdle(*Match);
	const FCytraPanelView Tower = Panel(*Match, Slot);
	TestEqual(TEXT("Tower title"), Tower.Title.ToString(), FString(TEXT("Baliste")));
	TestEqual(TEXT("Tower level"), Tower.TitleNote.ToString(), FString(TEXT("niveau 1/3")));
	TestTrue(TEXT("No tag on a plain slot"), Tower.Tag.IsEmpty());
	TestEqual(TEXT("Damage"), StatText(Tower, 0), FString(TEXT("Dégâts 15")));
	TestEqual(TEXT("Range"), StatText(Tower, 1), FString::Printf(TEXT("Portée %s cases"), *Decimal(2.8, 1)));
	TestEqual(TEXT("Rate"), StatText(Tower, 2), FString::Printf(TEXT("Cadence %s tir/s"), *Decimal(1.1, 2)));
	TestEqual(TEXT("Invested"), StatText(Tower, 3), FString(TEXT("Investi 50 or")));
	if (TestEqual(TEXT("Upgrade and dismantle"), Tower.Actions.Num(), 2))
	{
		const FCytraPanelAction& Upgrade = Tower.Actions[0];
		TestEqual(TEXT("Upgrade name"), Upgrade.Name.ToString(), FString(TEXT("Améliorer au niveau 2")));
		TestEqual(TEXT("Upgrade cost"), Upgrade.Cost.ToString(), FString(TEXT("35 or")));
		TestEqual(TEXT("Upgrade details"), Upgrade.Meta.ToString(), FString(TEXT("dégâts +60 %, portée +8 % · ingénieur · 5 s de chantier")));
		TestEqual(TEXT("Upgrade needs an engineer"), Upgrade.Refusal.ToString(), FString(TEXT("Il faut un ingénieur dans l’équipe")));
		const FCytraPanelAction& Dismantle = Tower.Actions[1];
		TestEqual(TEXT("Dismantle name"), Dismantle.Name.ToString(), FString(TEXT("Démonter")));
		TestEqual(TEXT("Dismantle price is the refund"), Dismantle.Cost.ToString(), FString(TEXT("+38 or")));
		TestEqual(TEXT("Dismantle details"), Dismantle.Meta.ToString(), FString(TEXT("rend 75 % de l’or investi · charpentier · 3 s de chantier")));
		TestTrue(TEXT("Dismantle accepted"), Dismantle.IsEnabled());

		Match->Order(Slot, Dismantle.Action);
		const FCytraPanelView Dismantling = Panel(*Match, Slot);
		TestTrue(TEXT("Dismantle ordered"), LineText(Dismantling, 0).StartsWith(TEXT("Démontage commandé. ")));
		TestTrue(TEXT("Stats stay during the job"), Dismantling.Stats.Num() == 4);
		TestTrue(TEXT("No order while dismantling"), Dismantling.Actions.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelTowerLevelsTest, "Cytra.UI.Panel.TowerLevels", CytraTest::Flags)

bool FCytraPanelTowerLevelsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Crew = { TEXT("Carpenter"), TEXT("Engineer") };
	TOptional<FCytraMatch> Match = MakeMatch(*this, &Crew);
	if (!Match)
	{
		return false;
	}

	// A ballista on the promontory behind the cliff (7,3).
	const FIntPoint High(8, 3);
	Match->Order(FIntPoint(7, 3), Work(TEXT("Ladder")));
	StepUntilIdle(*Match);
	Match->Order(High, FCytraOrderAction::Build(TEXT("Ballista")));
	StepUntilIdle(*Match);
	const FCytraPanelView Level1 = Panel(*Match, High);
	TestEqual(TEXT("Promontory tag"), Level1.Tag.ToString(), FString(TEXT("Promontoire, portée +30 %")));
	TestEqual(TEXT("Range on a promontory"), StatText(Level1, 1), FString::Printf(TEXT("Portée %s cases"), *Decimal(3.64, 1)));

	// During an upgrade, then at the last level. A fresh match: the ladder leaves too little gold for two upgrades.
	Match = MakeMatch(*this, &Crew);
	if (!Match)
	{
		return false;
	}
	const FIntPoint Plain(3, 2);
	Match->Order(Plain, FCytraOrderAction::Build(TEXT("Ballista")));
	StepUntilIdle(*Match);
	Match->Order(Plain, FCytraOrderAction::Upgrade());
	TestTrue(TEXT("Upgrade ordered"), LineText(Panel(*Match, Plain), 0).StartsWith(TEXT("Amélioration commandée. ")));
	StepUntilIdle(*Match);
	TestEqual(TEXT("Level 2"), Panel(*Match, Plain).TitleNote.ToString(), FString(TEXT("niveau 2/3")));
	TestEqual(TEXT("Next upgrade"), Panel(*Match, Plain).Actions[0].Name.ToString(), FString(TEXT("Améliorer au niveau 3")));
	Match->Order(Plain, FCytraOrderAction::Upgrade());
	StepUntilIdle(*Match);

	const FCytraPanelView Level3 = Panel(*Match, Plain);
	TestEqual(TEXT("Level 3"), Level3.TitleNote.ToString(), FString(TEXT("niveau 3/3")));
	TestEqual(TEXT("Damage at level 3"), StatText(Level3, 0), FString(TEXT("Dégâts 33")));
	TestEqual(TEXT("Invested at level 3"), StatText(Level3, 3), FString(TEXT("Investi 155 or")));
	TestEqual(TEXT("Max level"), LineText(Level3, 0), FString(TEXT("Niveau maximal atteint.")));
	if (TestEqual(TEXT("Only the dismantling left"), Level3.Actions.Num(), 1))
	{
		TestEqual(TEXT("Refund of a level 3 ballista"), Level3.Actions[0].Cost.ToString(), FString(TEXT("+116 or")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelMineTest, "Cytra.UI.Panel.Mine", CytraTest::Flags)

bool FCytraPanelMineTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	const FIntPoint Mine(14, 11);
	const FString Recall = TEXT(" Un déblayage commandé rappelle un mineur.");

	// Closed: the order to open it, then the job.
	const FCytraPanelView Closed = Panel(*Match, Mine);
	if (!TestEqual(TEXT("Open order"), Closed.Actions.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("Open name"), Closed.Actions[0].Name.ToString(), FString(TEXT("Ouvrir la mine")));
	TestEqual(TEXT("Open cost"), Closed.Actions[0].Cost.ToString(), FString(TEXT("50 or")));
	TestEqual(TEXT("Open details"), Closed.Actions[0].Meta.ToString(), FString(TEXT("mineur · 8 s de chantier")));
	Match->Order(Mine, Closed.Actions[0].Action);
	TestEqual(TEXT("Opening ordered"), LineText(Panel(*Match, Mine), 1), FString(TEXT("Ouverture commandée. Un mineur est en route.")));

	// Open, with the miner who opened it at work: one place left.
	StepUntilIdle(*Match);
	const FCytraPanelView Open = Panel(*Match, Mine);
	TestEqual(TEXT("Open title"), Open.Title.ToString(), FString(TEXT("Mine d’or")));
	TestEqual(TEXT("Miners"), Open.TitleNote.ToString(), FString(TEXT("1/2 mineurs")));
	TestEqual(TEXT("Income"), StatText(Open, 0), FString(TEXT("Revenu 2 or toutes les 6 s")));
	TestEqual(TEXT("Extracted"), StatText(Open, 1), FString(TEXT("Extrait 0 or")));
	TestEqual(TEXT("One place left"), LineText(Open, 0), FString(TEXT("Il reste une place : un second mineur doublerait le revenu.")) + Recall);
	TestTrue(TEXT("No order on an open mine"), Open.Actions.IsEmpty());
	for (int32 i = 0; i < 61 * 6; ++i)
	{
		Match->Step(Dt);
	}
	TestEqual(TEXT("Extracted after 6 s"), StatText(Panel(*Match, Mine), 1), FString(TEXT("Extrait 2 or")));

	// A clearing recalls the miner: nobody is left, then he walks back down.
	Match->Order(FIntPoint(13, 8), Work(TEXT("Clear")));
	const FCytraPanelView Empty = Panel(*Match, Mine);
	TestEqual(TEXT("No miner"), Empty.TitleNote.ToString(), FString(TEXT("0/2 mineurs")));
	TestEqual(TEXT("No income"), StatText(Empty, 0), FString(TEXT("Revenu 0 or toutes les 6 s")));
	TestEqual(TEXT("Nobody"), LineText(Empty, 0), FString(TEXT("Personne à la mine. Un mineur y descend dès qu’il n’a plus de chantier.")) + Recall);
	StepUntilIdle(*Match);
	TestEqual(TEXT("On the way back"), LineText(Panel(*Match, Mine), 0),
		FString(TEXT("Un mineur est en chemin. Il reste une place : un second mineur doublerait le revenu.")) + Recall);

	// Two miners fill it.
	const TArray<FName> Miners = { TEXT("Miner"), TEXT("Miner") };
	Match = MakeMatch(*this, &Miners);
	if (!Match)
	{
		return false;
	}
	Match->Order(Mine, FCytraOrderAction::Work(TEXT("OpenMine")));
	StepUntilIdle(*Match);
	for (int32 i = 0; i < 60 * 20; ++i)
	{
		Match->Step(Dt);
	}
	const FCytraPanelView Full = Panel(*Match, Mine);
	TestEqual(TEXT("Two miners"), Full.TitleNote.ToString(), FString(TEXT("2/2 mineurs")));
	TestEqual(TEXT("Double income"), StatText(Full, 0), FString(TEXT("Revenu 4 or toutes les 6 s")));
	TestEqual(TEXT("Full"), LineText(Full, 0), FString(TEXT("Les 2 places sont prises.")) + Recall);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraPanelCrewTest, "Cytra.UI.Panel.Crew", CytraTest::Flags)

bool FCytraPanelCrewTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraPanelView Start = FCytraPanelView::BuildCrew(*Match);
	TestEqual(TEXT("Title"), Start.Title.ToString(), FString(TEXT("Équipe")));
	TestEqual(TEXT("Size"), Start.TitleNote.ToString(), FString(TEXT("2/8")));
	if (!TestEqual(TEXT("One button per specialty"), Start.Actions.Num(), 3))
	{
		return false;
	}
	const FCytraPanelAction& Miner = Start.Actions[0];
	TestEqual(TEXT("Recruits a miner"), Miner.Recruit, FName(TEXT("Miner")));
	TestEqual(TEXT("Miner name"), Miner.Name.ToString(), FString(TEXT("Recruter un mineur")));
	TestEqual(TEXT("Miner cost"), Miner.Cost.ToString(), FString(TEXT("40 or")));
	TestEqual(TEXT("Miner details"), Miner.Meta.ToString(), FString(TEXT("Spécialité : déblayage, mine d’or")));
	TestTrue(TEXT("Miner recruitable"), Miner.IsEnabled());
	TestEqual(TEXT("Engineer name"), Start.Actions[2].Name.ToString(), FString(TEXT("Recruter un ingénieur")));
	TestEqual(TEXT("Engineer details"), Start.Actions[2].Meta.ToString(), FString(TEXT("Spécialité : ingénierie")));

	// 170 gold: an engineer, a miner and a carpenter leave 20, short of 30 for another carpenter.
	Match->Recruit(TEXT("Engineer"));
	Match->Recruit(TEXT("Miner"));
	Match->Recruit(TEXT("Carpenter"));
	const FCytraPanelView Poor = FCytraPanelView::BuildCrew(*Match);
	TestEqual(TEXT("Size after recruiting"), Poor.TitleNote.ToString(), FString(TEXT("5/8")));
	TestEqual(TEXT("Carpenter too expensive"), Poor.Actions[1].Refusal.ToString(), FString(TEXT("Il manque 30 or")));
	TestNotEqual(TEXT("Signature changes"), Poor.Signature(), Start.Signature());

	const TArray<FName> Full = { TEXT("Carpenter"), TEXT("Miner"), TEXT("Engineer"), TEXT("Carpenter"), TEXT("Miner"), TEXT("Engineer"), TEXT("Carpenter"), TEXT("Miner") };
	Match = MakeMatch(*this, &Full);
	if (!Match)
	{
		return false;
	}
	const FCytraPanelView Complete = FCytraPanelView::BuildCrew(*Match);
	TestEqual(TEXT("Full size"), Complete.TitleNote.ToString(), FString(TEXT("8/8")));
	TestEqual(TEXT("Crew full"), Complete.Actions[0].Refusal.ToString(), FString(TEXT("Équipe complète (8)")));
	return true;
}

} // namespace CytraPanelTest

#endif
