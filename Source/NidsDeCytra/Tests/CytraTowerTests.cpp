// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraTowerTest
{
	constexpr double Dt = 1.0 / 60.0;

	const FName Ballista(TEXT("Ballista"));
	const FName Carpenter(TEXT("Carpenter"));
	const FName Engineer(TEXT("Engineer"));

	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test, const TArray<FName>& StartRoles)
	{
		FCytraMapDef Map;
		FCytraBalance Balance;
		FCytraWaveSchedule Waves;
		if (!CytraTest::LoadAll(Test, Map, Balance, Waves))
		{
			return {};
		}
		Balance.StartRoles = StartRoles;
		return FCytraMatch(Map, Balance, Waves);
	}

	void StepFor(FCytraMatch& Match, double Seconds)
	{
		for (int32 i = 0; i < FMath::RoundToInt32(Seconds / Dt); ++i)
		{
			Match.Step(Dt);
		}
	}

	/** Steps until a job is done, for at most MaxSeconds. Returns that job's event. */
	TOptional<FCytraMatchEvent> StepUntilJobDone(FCytraMatch& Match, double MaxSeconds = 60.0)
	{
		for (int32 i = 0; i < FMath::RoundToInt32(MaxSeconds / Dt); ++i)
		{
			Match.Step(Dt);
			for (const FCytraMatchEvent& Event : Match.ConsumeEvents())
			{
				if (Event.Type == ECytraMatchEventType::JobDone)
				{
					return Event;
				}
			}
		}
		return {};
	}

	/** Orders on Cell and waits for the job to be done. Returns its event. */
	TOptional<FCytraMatchEvent> OrderAndWait(FAutomationTestBase& Test, FCytraMatch& Match, const FIntPoint& Cell, const FCytraOrderAction& Action)
	{
		if (!Test.TestTrue(TEXT("Order accepted"), Match.Order(Cell, Action).IsAccepted()))
		{
			return {};
		}
		TOptional<FCytraMatchEvent> Done = StepUntilJobDone(Match);
		Test.TestTrue(TEXT("Job done"), Done.IsSet());
		return Done;
	}

	TArray<FIntPoint> TowerCells(const FCytraMatch& Match)
	{
		TArray<FIntPoint> Cells;
		for (const FCytraTowerGun& Gun : Match.GetTowers())
		{
			Cells.Add(Gun.Cell);
		}
		return Cells;
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTowersLifecycleTest, "Cytra.Rules.Towers.Lifecycle", CytraTest::Flags)

bool FCytraTowersLifecycleTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this, { Carpenter, Engineer });
	if (!Match)
	{
		return false;
	}
	const FIntPoint Slot(3, 2);

	// Build: paid at the order, the tower appears at the end of the job.
	TestTrue(TEXT("Build accepted"), Match->Order(Slot, FCytraOrderAction::Build(Ballista)).IsAccepted());
	TestEqual(TEXT("Gold after the build order"), Match->GetWallet().Gold, 120);
	TestNull(TEXT("No tower while building"), Match->GetGrid().GetTower(Slot));
	TOptional<FCytraMatchEvent> Built = StepUntilJobDone(*Match);
	if (!TestTrue(TEXT("Build done"), Built.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Build event"), Built->Job == ECytraOrderKind::Build);
	TestEqual(TEXT("Built type"), Built->Work, Ballista);
	TestEqual(TEXT("Built level"), Built->Level, 1);
	TestEqual(TEXT("Tower in service"), TowerCells(*Match), TArray<FIntPoint>{ Slot });
	TestEqual(TEXT("Invested"), Match->GetGrid().GetTower(Slot)->Invested, 50);

	// Two upgrades by the engineer: 35 then 70 gold.
	TOptional<FCytraMatchEvent> Level2 = OrderAndWait(*this, *Match, Slot, FCytraOrderAction::Upgrade());
	TOptional<FCytraMatchEvent> Level3 = OrderAndWait(*this, *Match, Slot, FCytraOrderAction::Upgrade());
	if (!Level2 || !Level3)
	{
		return false;
	}
	TestTrue(TEXT("Upgrade event"), Level2->Job == ECytraOrderKind::Upgrade);
	TestEqual(TEXT("Level 2"), Level2->Level, 2);
	TestEqual(TEXT("Level 3"), Level3->Level, 3);
	// GDD case 7: a level 3 ballista cost 155 gold.
	TestEqual(TEXT("Invested at level 3"), Match->GetGrid().GetTower(Slot)->Invested, 155);
	TestEqual(TEXT("Gold after the upgrades"), Match->GetWallet().Gold, 15);
	TestEqual(TEXT("Still one tower"), Match->GetTowers().Num(), 1);

	// Dismantle: free, and the refund is paid only at the end of the job.
	TestTrue(TEXT("Dismantle accepted"), Match->Order(Slot, FCytraOrderAction::Dismantle()).IsAccepted());
	StepFor(*Match, 1.0);
	TestEqual(TEXT("No refund before the end"), Match->GetWallet().Gold, 15);
	TestNotNull(TEXT("Tower stands during the dismantling"), Match->GetGrid().GetTower(Slot));
	TOptional<FCytraMatchEvent> Dismantled = StepUntilJobDone(*Match);
	if (!TestTrue(TEXT("Dismantle done"), Dismantled.IsSet()))
	{
		return false;
	}
	TestTrue(TEXT("Dismantle event"), Dismantled->Job == ECytraOrderKind::Dismantle);
	// GDD case 7: dismantling it gives back 116 gold.
	TestEqual(TEXT("Refund in the event"), Dismantled->Refund, 116);
	TestEqual(TEXT("Gold after the refund"), Match->GetWallet().Gold, 131);
	TestEqual(TEXT("Refund counted"), Match->GetWallet().Refunded, 116);
	TestNull(TEXT("Slot free"), Match->GetGrid().GetTower(Slot));
	TestEqual(TEXT("No tower in service"), Match->GetTowers().Num(), 0);
	TestTrue(TEXT("Slot buildable again"), Match->CheckOrder(Slot, FCytraOrderAction::Build(Ballista)).IsAccepted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTowersFiringOrderTest, "Cytra.Rules.Towers.FiringOrder", CytraTest::Flags)

bool FCytraTowersFiringOrderTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this, { Carpenter });
	if (!Match)
	{
		return false;
	}
	const FIntPoint First(3, 2);
	const FIntPoint Second(11, 4);
	OrderAndWait(*this, *Match, First, FCytraOrderAction::Build(Ballista));
	OrderAndWait(*this, *Match, Second, FCytraOrderAction::Build(Ballista));
	TestEqual(TEXT("Order of construction"), TowerCells(*Match), TArray<FIntPoint>{ First, Second });

	// A tower rebuilt on a freed slot fires after the ones already standing.
	OrderAndWait(*this, *Match, First, FCytraOrderAction::Dismantle());
	OrderAndWait(*this, *Match, First, FCytraOrderAction::Build(Ballista));
	TestEqual(TEXT("Rebuilt tower comes last"), TowerCells(*Match), TArray<FIntPoint>{ Second, First });
	return true;
}

}

#endif
