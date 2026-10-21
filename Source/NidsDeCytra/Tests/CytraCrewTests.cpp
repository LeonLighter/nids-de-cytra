// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraCrewTest
{
	constexpr double Dt = 1.0 / 60.0;

	const FName Carpenter(TEXT("Carpenter"));
	const FName Miner(TEXT("Miner"));

	/** A match on the real data, optionally with another starting crew. */
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

	void StepFor(FCytraMatch& Match, double Seconds)
	{
		const int32 Steps = FMath::RoundToInt32(Seconds / Dt);
		for (int32 i = 0; i < Steps; ++i)
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

	const FCytraWorker& WorkerOfRole(const FCytraMatch& Match, FName Role)
	{
		return *Match.GetWorkers().FindByPredicate([Role](const FCytraWorker& Worker) { return Worker.Role == Role; });
	}

	FCytraOrderAction Work(const TCHAR* Id)
	{
		return FCytraOrderAction::Work(FName(Id));
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewStartTest, "Cytra.Rules.Crew.Start", CytraTest::Flags)

bool FCytraCrewStartTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const TArray<FCytraWorker>& Workers = Match->GetWorkers();
	if (!TestEqual(TEXT("Crew size"), Workers.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("First worker"), Workers[0].Role, Carpenter);
	TestEqual(TEXT("Second worker"), Workers[1].Role, Miner);
	for (const FCytraWorker& Worker : Workers)
	{
		TestEqual(TEXT("At the main camp"), Worker.Position, FVector2d(20.0, 9.0));
		TestTrue(TEXT("Idle"), Worker.State == ECytraWorkerState::Idle);
	}
	TestTrue(TEXT("No job"), Match->GetJobs().IsEmpty());

	// Nobody moves without orders.
	StepFor(*Match, 5.0);
	TestEqual(TEXT("Still at the camp"), Match->GetWorkers()[0].Position, FVector2d(20.0, 9.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewOrderTest, "Cytra.Rules.Crew.Order", CytraTest::Flags)

bool FCytraCrewOrderTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FIntPoint Gap(9, 6);
	TestTrue(TEXT("Wrong work refused"), Match->Order(Gap, Work(TEXT("Clear"))).Check.Refusal == ECytraOrderRefusal::NotAvailableHere);

	const FCytraOrderResult Result = Match->Order(Gap, Work(TEXT("Bridge")));
	if (!TestTrue(TEXT("Bridge accepted"), Result.IsAccepted()))
	{
		return false;
	}
	TestEqual(TEXT("Gold paid on order"), Match->GetWallet().Gold, 110);
	TestEqual(TEXT("Spent"), Match->GetWallet().Spent, 60);
	TestFalse(TEXT("Not waiting"), Result.bWaiting);

	const FCytraJob* Job = Match->FindJobAt(Gap);
	const FCytraWorker& Worker = WorkerOfRole(*Match, Carpenter);
	if (!TestNotNull(TEXT("Job queued"), Job))
	{
		return false;
	}
	TestEqual(TEXT("Carpenter assigned"), Job->Worker, Worker.Id);
	TestEqual(TEXT("Job taken"), Worker.Job, Job->Id);
	TestTrue(TEXT("On its way"), Worker.State == ECytraWorkerState::Move);
	// The only walkable neighbour of the gap is the road at (9,5), 15 cells from the camp.
	TestEqual(TEXT("Path length"), Worker.Path.Num(), 15);
	TestEqual(TEXT("Stands next to the gap"), Worker.Path.Last(), FIntPoint(9, 5));

	const FCytraOrderResult Again = Match->Order(Gap, Work(TEXT("Bridge")));
	TestTrue(TEXT("One job per cell"), Again.Check.Refusal == ECytraOrderRefusal::JobInProgress);
	TestEqual(TEXT("Refusal costs nothing"), Match->GetWallet().Gold, 110);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewBridgeTimingTest, "Cytra.Rules.Crew.BridgeTiming", CytraTest::Flags)

bool FCytraCrewBridgeTimingTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match || !TestTrue(TEXT("Bridge accepted"), Match->Order(FIntPoint(9, 6), Work(TEXT("Bridge"))).IsAccepted()))
	{
		return false;
	}

	// 15 cells at 3.2 cells/s: on site during step 282 (4.6875 s), then 9 s of work from step 283.
	for (int32 i = 0; i < 281; ++i)
	{
		Match->Step(Dt);
	}
	TestTrue(TEXT("Still walking after 281 steps"), WorkerOfRole(*Match, Carpenter).State == ECytraWorkerState::Move);
	Match->Step(Dt);
	TestTrue(TEXT("At work after 282 steps"), WorkerOfRole(*Match, Carpenter).State == ECytraWorkerState::Work);
	TestEqual(TEXT("On site"), WorkerOfRole(*Match, Carpenter).Position, FVector2d(9.0, 5.0));

	for (int32 i = 0; i < 539; ++i)
	{
		Match->Step(Dt);
	}
	TestFalse(TEXT("Not done after 821 steps"), Match->GetGrid().IsOpened(FIntPoint(9, 6)));
	Match->ConsumeEvents();
	Match->Step(Dt);
	Match->Step(Dt);
	TestTrue(TEXT("Done after 823 steps"), Match->GetGrid().IsOpened(FIntPoint(9, 6)));
	TestTrue(TEXT("Job removed"), Match->GetJobs().IsEmpty());

	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	const FCytraMatchEvent* Done = Events.FindByPredicate([](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::JobDone; });
	if (TestNotNull(TEXT("Job done event"), Done))
	{
		TestEqual(TEXT("Work"), Done->Work, FName(TEXT("Bridge")));
		TestEqual(TEXT("Cell"), Done->Cell, FIntPoint(9, 6));
		TestEqual(TEXT("Two slots opened"), Done->OpenedSlots, 2);
	}
	TestEqual(TEXT("7 slots reachable"), Match->GetGrid().ReachableSlotCount(), 7);

	const FCytraWorker& Worker = WorkerOfRole(*Match, Carpenter);
	TestEqual(TEXT("Job released"), Worker.Job, 0);
	TestTrue(TEXT("Walks back to the camp"), Worker.State == ECytraWorkerState::Back);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewWaitingJobTest, "Cytra.Rules.Crew.WaitingJob", CytraTest::Flags)

bool FCytraCrewWaitingJobTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraOrderResult Bridge = Match->Order(FIntPoint(9, 6), Work(TEXT("Bridge")));
	const FCytraOrderResult Ladder = Match->Order(FIntPoint(7, 3), Work(TEXT("Ladder")));
	if (!TestTrue(TEXT("Both accepted"), Bridge.IsAccepted() && Ladder.IsAccepted()))
	{
		return false;
	}
	TestTrue(TEXT("Ladder waits for the only carpenter"), Ladder.bWaiting);
	TestEqual(TEXT("Both paid"), Match->GetWallet().Gold, 170 - 60 - 45);

	const TOptional<FCytraMatchEvent> Done = StepUntilJobDone(*Match);
	if (!TestTrue(TEXT("Bridge done"), Done.IsSet() && Done->Cell == FIntPoint(9, 6)))
	{
		return false;
	}
	const FCytraJob* Next = Match->FindJobAt(FIntPoint(7, 3));
	const FCytraWorker& Worker = WorkerOfRole(*Match, Carpenter);
	if (TestNotNull(TEXT("Ladder still queued"), Next))
	{
		TestEqual(TEXT("Taken as soon as the carpenter is free"), Next->Worker, Worker.Id);
	}
	TestTrue(TEXT("On its way"), Worker.State == ECytraWorkerState::Move);

	const TOptional<FCytraMatchEvent> LadderDone = StepUntilJobDone(*Match);
	TestTrue(TEXT("Ladder done"), LadderDone.IsSet() && LadderDone->Cell == FIntPoint(7, 3));
	TestTrue(TEXT("Promontory (8,3) reachable"), Match->GetGrid().IsReachable(FIntPoint(8, 3)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewClearBeforeLadderTest, "Cytra.Rules.Crew.ClearBeforeLadder", CytraTest::Flags)

bool FCytraCrewClearBeforeLadderTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FIntPoint Cliff(14, 7);
	TestTrue(TEXT("Cliff unreachable before clearing"), Match->CheckOrder(Cliff, Work(TEXT("Ladder"))).Refusal == ECytraOrderRefusal::Unreachable);

	const FCytraOrderResult Clear = Match->Order(FIntPoint(13, 8), Work(TEXT("Clear")));
	if (!TestTrue(TEXT("Clearing accepted"), Clear.IsAccepted()))
	{
		return false;
	}
	TestEqual(TEXT("The miner takes it"), Match->FindJobAt(FIntPoint(13, 8))->Worker, WorkerOfRole(*Match, Miner).Id);

	const TOptional<FCytraMatchEvent> Done = StepUntilJobDone(*Match);
	if (!TestTrue(TEXT("Clearing done"), Done.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("One slot opened"), Done->OpenedSlots, 1);
	TestEqual(TEXT("6 slots reachable"), Match->GetGrid().ReachableSlotCount(), 6);
	TestTrue(TEXT("Ladder now accepted"), Match->CheckOrder(Cliff, Work(TEXT("Ladder"))).IsAccepted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewNearestWorkerTest, "Cytra.Rules.Crew.NearestWorker", CytraTest::Flags)

bool FCytraCrewNearestWorkerTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Roles = { Carpenter, Carpenter, Miner };
	TOptional<FCytraMatch> Match = MakeMatch(*this, &Roles);
	if (!Match)
	{
		return false;
	}
	const int32 First = Match->GetWorkers()[0].Id;
	const int32 Second = Match->GetWorkers()[1].Id;

	Match->Order(FIntPoint(9, 6), Work(TEXT("Bridge")));
	TestEqual(TEXT("Tie goes to the first worker"), Match->FindJobAt(FIntPoint(9, 6))->Worker, First);
	if (!TestTrue(TEXT("Bridge done"), StepUntilJobDone(*Match).IsSet()))
	{
		return false;
	}

	// Ten steps into its walk back from (9,5), the first carpenter is between (9,5) and (10,5).
	for (int32 i = 0; i < 10; ++i)
	{
		Match->Step(Dt);
	}
	const FVector2d Before = Match->FindWorker(First)->Position;
	TestTrue(TEXT("Between two cells"), Before.X > 9.5 && Before.X < 10.0 && Before.Y == 5.0);

	Match->Order(FIntPoint(7, 3), Work(TEXT("Ladder")));
	TestEqual(TEXT("The nearest carpenter is chosen"), Match->FindJobAt(FIntPoint(7, 3))->Worker, First);
	TestEqual(TEXT("The other stays at the camp"), Match->FindWorker(Second)->Job, 0);

	const FCytraWorker& Worker = *Match->FindWorker(First);
	if (TestEqual(TEXT("Path: back to its cell centre, then 6 cells"), Worker.Path.Num(), 7))
	{
		TestEqual(TEXT("Centre of its cell first"), Worker.Path[0], FIntPoint(10, 5));
		TestEqual(TEXT("Stands next to the cliff"), Worker.Path.Last(), FIntPoint(6, 3));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewNearestCampTest, "Cytra.Rules.Crew.NearestCamp", CytraTest::Flags)

bool FCytraCrewNearestCampTest::RunTest(const FString& Parameters)
{
	// Without the outpost, a carpenter finishing in the west walks back to the main camp.
	{
		TOptional<FCytraMatch> Match = MakeMatch(*this);
		if (!Match)
		{
			return false;
		}
		Match->Order(FIntPoint(7, 3), Work(TEXT("Ladder")));
		TestTrue(TEXT("Ladder done"), StepUntilJobDone(*Match).IsSet());
		StepFor(*Match, 10.0);
		const FCytraWorker& Worker = WorkerOfRole(*Match, Carpenter);
		TestEqual(TEXT("Back at the main camp"), Worker.Position, FVector2d(20.0, 9.0));
		TestTrue(TEXT("Idle"), Worker.State == ECytraWorkerState::Idle);
	}

	// With the outpost raised, it waits there instead.
	{
		TOptional<FCytraMatch> Match = MakeMatch(*this);
		if (!Match)
		{
			return false;
		}
		const FIntPoint Outpost(4, 7);
		TestTrue(TEXT("Outpost accepted"), Match->Order(Outpost, Work(TEXT("Outpost"))).IsAccepted());
		TestTrue(TEXT("Outpost raised"), StepUntilJobDone(*Match).IsSet());
		TestTrue(TEXT("Outpost is a camp"), Match->GetGrid().IsCamp(Outpost));
		StepFor(*Match, 2.0);
		TestEqual(TEXT("Steps onto the outpost"), WorkerOfRole(*Match, Carpenter).Position, FVector2d(4.0, 7.0));

		Match->Order(FIntPoint(7, 3), Work(TEXT("Ladder")));
		TestTrue(TEXT("Ladder done"), StepUntilJobDone(*Match).IsSet());
		StepFor(*Match, 10.0);
		const FCytraWorker& Worker = WorkerOfRole(*Match, Carpenter);
		TestEqual(TEXT("Back at the outpost"), Worker.Position, FVector2d(4.0, 7.0));
		TestTrue(TEXT("Idle"), Worker.State == ECytraWorkerState::Idle);
		TestEqual(TEXT("Gold"), Match->GetWallet().Gold, 170 - 70 - 45);
	}
	return true;
}

} // namespace CytraCrewTest

#endif
