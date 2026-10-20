// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraGrid.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraOrders.h"
#include "Rules/CytraWallet.h"

namespace
{
	const FName Miner(TEXT("Miner"));
	const FName Carpenter(TEXT("Carpenter"));
	const FName Engineer(TEXT("Engineer"));

	/** A crew with every specialty and plenty of gold: only the grid can refuse. */
	FCytraOrderContext Unlimited()
	{
		FCytraOrderContext Context;
		Context.CrewRoles = { Miner, Carpenter, Engineer };
		Context.Gold = 10000;
		return Context;
	}

	void TestRefusal(FAutomationTestBase& Test, const TCHAR* What, const FCytraOrderCheck& Check, ECytraOrderRefusal Expected)
	{
		Test.TestEqual(What, static_cast<int32>(Check.Refusal), static_cast<int32>(Expected));
	}
}

// GDD section 13, case 5.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraOrdersCliffNeedsRubbleFirstTest, "Cytra.Rules.Orders.CliffNeedsRubbleFirst", CytraTest::Flags)

bool FCytraOrdersCliffNeedsRubbleFirstTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}
	FCytraGrid Grid(Map);
	const FIntPoint Cliff(14, 7);
	const FCytraOrderAction Ladder = FCytraOrderAction::Work(TEXT("Ladder"));

	const FCytraOrderCheck Before = FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Unlimited());
	TestRefusal(*this, TEXT("Ladder before clearing"), Before, ECytraOrderRefusal::Unreachable);
	TestTrue(TEXT("The ladder still applies to the cliff"), Before.Spec.IsSet());

	Grid.Open(FIntPoint(13, 8));
	TestTrue(TEXT("Ladder after clearing"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Unlimited()).IsAccepted());
	return true;
}

// GDD section 13, case 11.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraOrdersNeedsSpecialtyTest, "Cytra.Rules.Orders.NeedsSpecialty", CytraTest::Flags)

bool FCytraOrdersNeedsSpecialtyTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}
	const FCytraGrid Grid(Map);

	FCytraOrderContext NoMiner;
	NoMiner.CrewRoles = { Carpenter, Engineer };
	NoMiner.Gold = 1000;
	const FCytraOrderCheck Check = FCytraOrders::Check(Grid, Balance, FIntPoint(13, 8), FCytraOrderAction::Work(TEXT("Clear")), NoMiner);
	TestRefusal(*this, TEXT("Clearing without a miner"), Check, ECytraOrderRefusal::MissingSpecialty);
	if (TestTrue(TEXT("Spec known"), Check.Spec.IsSet()))
	{
		TestEqual(TEXT("Required specialty"), Check.Spec->Role, Miner);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraOrdersRefusalOrderTest, "Cytra.Rules.Orders.RefusalOrder", CytraTest::Flags)

bool FCytraOrdersRefusalOrderTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}
	FCytraGrid Grid(Map);
	const FIntPoint Cliff(14, 7);
	const FCytraOrderAction Ladder = FCytraOrderAction::Work(TEXT("Ladder"));

	// Every check fails at first; lift them one by one, in the order of GDD section 5.
	FCytraOrderContext Context;
	Context.bGameOver = true;
	Context.bJobOnCell = true;

	TestRefusal(*this, TEXT("Wrong action comes first"), FCytraOrders::Check(Grid, Balance, Cliff, FCytraOrderAction::Work(TEXT("Bridge")), Context), ECytraOrderRefusal::NotAvailableHere);
	TestRefusal(*this, TEXT("Game over"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context), ECytraOrderRefusal::GameOver);
	Context.bGameOver = false;
	TestRefusal(*this, TEXT("Job in progress"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context), ECytraOrderRefusal::JobInProgress);
	Context.bJobOnCell = false;
	TestRefusal(*this, TEXT("Unreachable"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context), ECytraOrderRefusal::Unreachable);
	Grid.Open(FIntPoint(13, 8));
	TestRefusal(*this, TEXT("Missing specialty"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context), ECytraOrderRefusal::MissingSpecialty);
	Context.CrewRoles = { Carpenter };
	Context.Gold = 40;
	const FCytraOrderCheck Short = FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context);
	TestRefusal(*this, TEXT("Not enough gold"), Short, ECytraOrderRefusal::NotEnoughGold);
	TestEqual(TEXT("Missing gold"), Short.MissingGold, 5);
	Context.Gold = 45;
	TestTrue(TEXT("Accepted with exactly enough gold"), FCytraOrders::Check(Grid, Balance, Cliff, Ladder, Context).IsAccepted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraOrdersWorksTest, "Cytra.Rules.Orders.Works", CytraTest::Flags)

bool FCytraOrdersWorksTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}
	FCytraGrid Grid(Map);

	const TOptional<FCytraOrderSpec> Bridge = FCytraOrders::Describe(Grid, Balance, FIntPoint(9, 6), FCytraOrderAction::Work(TEXT("Bridge")));
	if (TestTrue(TEXT("Bridge on a gap"), Bridge.IsSet()))
	{
		TestEqual(TEXT("Bridge role"), Bridge->Role, Carpenter);
		TestEqual(TEXT("Bridge cost"), Bridge->Cost, 60);
		TestEqual(TEXT("Bridge time"), Bridge->Time, 9.0);
	}
	TestFalse(TEXT("No clearing on a gap"), FCytraOrders::Describe(Grid, Balance, FIntPoint(9, 6), FCytraOrderAction::Work(TEXT("Clear"))).IsSet());
	TestFalse(TEXT("Unknown work"), FCytraOrders::Describe(Grid, Balance, FIntPoint(9, 6), FCytraOrderAction::Work(TEXT("Tunnel"))).IsSet());
	TestTrue(TEXT("Bridge accepted: the gap touches the road"), FCytraOrders::Check(Grid, Balance, FIntPoint(9, 6), FCytraOrderAction::Work(TEXT("Bridge")), Unlimited()).IsAccepted());

	const FIntPoint Mine(14, 11);
	TestTrue(TEXT("Open the mine accepted"), FCytraOrders::Check(Grid, Balance, Mine, FCytraOrderAction::Work(TEXT("OpenMine")), Unlimited()).IsAccepted());
	Grid.Open(Mine);
	TestRefusal(*this, TEXT("Mine already open"), FCytraOrders::Check(Grid, Balance, Mine, FCytraOrderAction::Work(TEXT("OpenMine")), Unlimited()), ECytraOrderRefusal::NotAvailableHere);

	const FIntPoint Outpost(4, 7);
	TestTrue(TEXT("Raise the outpost accepted"), FCytraOrders::Check(Grid, Balance, Outpost, FCytraOrderAction::Work(TEXT("Outpost")), Unlimited()).IsAccepted());
	Grid.Open(Outpost);
	TestRefusal(*this, TEXT("Outpost already raised"), FCytraOrders::Check(Grid, Balance, Outpost, FCytraOrderAction::Work(TEXT("Outpost")), Unlimited()), ECytraOrderRefusal::NotAvailableHere);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraOrdersTowersTest, "Cytra.Rules.Orders.Towers", CytraTest::Flags)

bool FCytraOrdersTowersTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}
	FCytraGrid Grid(Map);
	const FIntPoint Slot(19, 6);

	TestRefusal(*this, TEXT("No tower on a road"), FCytraOrders::Check(Grid, Balance, FIntPoint(19, 7), FCytraOrderAction::Build(TEXT("Ballista")), Unlimited()), ECytraOrderRefusal::NotAvailableHere);
	TestRefusal(*this, TEXT("Slot out of reach"), FCytraOrders::Check(Grid, Balance, FIntPoint(15, 7), FCytraOrderAction::Build(TEXT("Ballista")), Unlimited()), ECytraOrderRefusal::Unreachable);
	TestRefusal(*this, TEXT("Nothing to upgrade"), FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Upgrade(), Unlimited()), ECytraOrderRefusal::NotAvailableHere);
	TestRefusal(*this, TEXT("Nothing to dismantle"), FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Dismantle(), Unlimited()), ECytraOrderRefusal::NotAvailableHere);

	const TOptional<FCytraOrderSpec> Coil = FCytraOrders::Describe(Grid, Balance, Slot, FCytraOrderAction::Build(TEXT("Coil")));
	if (TestTrue(TEXT("Coil buildable"), Coil.IsSet()))
	{
		TestEqual(TEXT("Coil built by an engineer"), Coil->Role, Engineer);
		TestEqual(TEXT("Coil cost"), Coil->Cost, 80);
	}

	// Build a ballista, upgrade it twice, then dismantle it: GDD section 13, case 7.
	const FCytraOrderCheck Build = FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Build(TEXT("Ballista")), Unlimited());
	if (!TestTrue(TEXT("Ballista accepted"), Build.IsAccepted()))
	{
		return false;
	}
	Grid.PlaceTower(Slot, Build.Spec->Id, Build.Spec->Cost);
	TestRefusal(*this, TEXT("Slot taken"), FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Build(TEXT("Mortar")), Unlimited()), ECytraOrderRefusal::NotAvailableHere);

	for (const int32 ExpectedCost : { 35, 70 })
	{
		const FCytraOrderCheck Upgrade = FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Upgrade(), Unlimited());
		if (!TestTrue(TEXT("Upgrade accepted"), Upgrade.IsAccepted()))
		{
			return false;
		}
		TestEqual(TEXT("Upgrade by an engineer"), Upgrade.Spec->Role, Engineer);
		TestEqual(TEXT("Upgrade cost"), Upgrade.Spec->Cost, ExpectedCost);
		TestEqual(TEXT("Upgrade time"), Upgrade.Spec->Time, 5.0);
		Grid.UpgradeTower(Slot, Upgrade.Spec->Cost);
	}
	TestEqual(TEXT("Level"), Grid.GetTower(Slot)->Level, 3);
	TestEqual(TEXT("Invested"), Grid.GetTower(Slot)->Invested, 155);
	TestRefusal(*this, TEXT("No level 4"), FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Upgrade(), Unlimited()), ECytraOrderRefusal::NotAvailableHere);

	const FCytraOrderCheck Dismantle = FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Dismantle(), Unlimited());
	if (TestTrue(TEXT("Dismantle accepted"), Dismantle.IsAccepted()))
	{
		TestEqual(TEXT("Dismantled by the builder's specialty"), Dismantle.Spec->Role, Carpenter);
		TestEqual(TEXT("Dismantle is free"), Dismantle.Spec->Cost, 0);
		TestEqual(TEXT("Dismantle time"), Dismantle.Spec->Time, 3.0);
		TestEqual(TEXT("Refund"), Dismantle.Spec->Refund, 116);
	}
	Grid.RemoveTower(Slot);
	TestTrue(TEXT("Slot free again"), FCytraOrders::Check(Grid, Balance, Slot, FCytraOrderAction::Build(TEXT("Mortar")), Unlimited()).IsAccepted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraEconomyWalletTest, "Cytra.Rules.Economy.Wallet", CytraTest::Flags)

bool FCytraEconomyWalletTest::RunTest(const FString& Parameters)
{
	FCytraWallet Wallet(170);
	TestTrue(TEXT("Pay 60"), Wallet.TrySpend(60));
	TestEqual(TEXT("Gold after paying"), Wallet.Gold, 110);
	TestEqual(TEXT("Missing for 150"), Wallet.Missing(150), 40);
	TestFalse(TEXT("Cannot pay 150"), Wallet.TrySpend(150));
	TestEqual(TEXT("Unchanged after a refusal"), Wallet.Gold, 110);
	TestEqual(TEXT("Spent"), Wallet.Spent, 60);

	Wallet.Earn(31);
	Wallet.EarnFromMine(2);
	Wallet.EarnRefund(116);
	TestEqual(TEXT("Gold"), Wallet.Gold, 259);
	TestEqual(TEXT("Mined"), Wallet.Mined, 2);
	TestEqual(TEXT("Refunded"), Wallet.Refunded, 116);
	TestEqual(TEXT("Missing when affordable"), Wallet.Missing(10), 0);
	return true;
}

#endif
