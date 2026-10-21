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
	TestTrue(TEXT("No tower orders before J40"), View.Actions.IsEmpty());

	const FCytraPanelView Reachable = Panel(*Match, FIntPoint(19, 6));
	TestEqual(TEXT("Reachable slot title"), Reachable.Title.ToString(), FString(TEXT("Emplacement")));
	TestTrue(TEXT("Reachable slot has no chain"), Reachable.Lines.IsEmpty());
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
	TestTrue(TEXT("Mine not opened before J50"), Mine.Actions.IsEmpty());

	const FCytraPanelView Outpost = Panel(*Match, FIntPoint(4, 7));
	TestEqual(TEXT("Outpost site"), Outpost.Title.ToString(), FString(TEXT("Site d’avant-poste")));
	if (TestEqual(TEXT("Outpost order"), Outpost.Actions.Num(), 1))
	{
		TestEqual(TEXT("Outpost order name"), Outpost.Actions[0].Name.ToString(), FString(TEXT("Monter l’avant-poste")));
		TestEqual(TEXT("Outpost order details"), Outpost.Actions[0].Meta.ToString(), FString(TEXT("charpentier · 10 s de chantier")));
	}
	return true;
}

} // namespace CytraPanelTest

#endif
