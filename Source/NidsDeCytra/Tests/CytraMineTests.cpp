// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraMineTest
{
	constexpr double Dt = 1.0 / 60.0;

	const FName Miner(TEXT("Miner"));
	const FName Carpenter(TEXT("Carpenter"));
	const FIntPoint Mine(14, 11);
	const FIntPoint Rubble(13, 8);

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
		for (int32 i = 0; i < FMath::RoundToInt32(Seconds / Dt); ++i)
		{
			Match.Step(Dt);
		}
	}

	/** Steps until no job is left, for at most a minute. */
	bool StepUntilIdle(FCytraMatch& Match)
	{
		for (int32 i = 0; i < 60 * 60 && !Match.GetJobs().IsEmpty(); ++i)
		{
			Match.Step(Dt);
		}
		return Match.GetJobs().IsEmpty();
	}

	/** Orders the mine open and waits for the job to be done. */
	bool OpenMine(FAutomationTestBase& Test, FCytraMatch& Match)
	{
		return Test.TestTrue(TEXT("Open mine accepted"), Match.Order(Mine, FCytraOrderAction::Work(TEXT("OpenMine"))).IsAccepted())
			&& Test.TestTrue(TEXT("Mine opened"), StepUntilIdle(Match));
	}

	int32 CountState(const FCytraMatch& Match, ECytraWorkerState State)
	{
		return Match.GetWorkers().FilterByPredicate([State](const FCytraWorker& Worker) { return Worker.State == State; }).Num();
	}

	const FCytraWorker* FindMiner(const FCytraMatch& Match, ECytraWorkerState State)
	{
		return Match.GetWorkers().FindByPredicate([State](const FCytraWorker& Worker) { return Worker.Role == Miner && Worker.State == State; });
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMineIncomeTest, "Cytra.Rules.Mine.Income", CytraTest::Flags)

bool FCytraMineIncomeTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	TestEqual(TEXT("No miner at a closed mine"), Match->MinersAtMine(), 0);
	if (!OpenMine(*this, *Match))
	{
		return false;
	}
	Match->ConsumeEvents();

	// The miner who opened it stands next to it, and goes down at once.
	TestEqual(TEXT("The miner goes down"), Match->MinersMining(), 1);
	const int32 GoldBefore = Match->GetWallet().Gold;

	// 2 gold every 6 seconds.
	StepFor(*Match, 5.9);
	TestEqual(TEXT("Nothing before 6 s"), Match->GetWallet().Mined, 0);
	StepFor(*Match, 0.2);
	TestEqual(TEXT("2 gold at 6 s"), Match->GetWallet().Mined, 2);
	TestEqual(TEXT("Gold earned"), Match->GetWallet().Gold, GoldBefore + 2);
	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	if (TestEqual(TEXT("One yield event"), Events.Num(), 1))
	{
		TestTrue(TEXT("Yield event"), Events[0].Type == ECytraMatchEventType::MineYield);
		TestEqual(TEXT("Yield"), Events[0].Gold, 2);
		TestEqual(TEXT("At the mine"), Events[0].Cell, Mine);
	}
	StepFor(*Match, 6.0);
	TestEqual(TEXT("4 gold at 12 s"), Match->GetWallet().Mined, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMinePlacesTest, "Cytra.Rules.Mine.Places", CytraTest::Flags)

bool FCytraMinePlacesTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Crew = { Miner, Miner, Miner };
	TOptional<FCytraMatch> Match = MakeMatch(*this, &Crew);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	TestEqual(TEXT("Two places taken at once"), Match->MinersAtMine(), 2);
	StepFor(*Match, 20.0);
	TestEqual(TEXT("Two miners extract"), Match->MinersMining(), 2);
	TestEqual(TEXT("The third waits at the camp"), CountState(*Match, ECytraWorkerState::Idle), 1);
	const FCytraWorker* Waiting = FindMiner(*Match, ECytraWorkerState::Idle);
	if (TestNotNull(TEXT("Waiting miner"), Waiting))
	{
		TestTrue(TEXT("On the main camp"), Waiting->Position == FVector2d(Match->GetMap().Camp.X, Match->GetMap().Camp.Y));
	}

	// A clearing takes the free miner rather than one at the mine.
	const int32 FreeMiner = Waiting ? Waiting->Id : 0;
	Match->Order(Rubble, FCytraOrderAction::Work(TEXT("Clear")));
	TestEqual(TEXT("The free miner is sent"), Match->FindJobAt(Rubble)->Worker, FreeMiner);
	TestEqual(TEXT("Both stay at the mine"), Match->MinersMining(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMineRecallTest, "Cytra.Rules.Mine.Recall", CytraTest::Flags)

bool FCytraMineRecallTest::RunTest(const FString& Parameters)
{
	// GDD case 12: with the mine open and a single miner, a clearing takes him from the mine; once done, he goes back.
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match || !OpenMine(*this, *Match))
	{
		return false;
	}
	StepFor(*Match, 3.0);
	const FCytraWorker* AtMine = FindMiner(*Match, ECytraWorkerState::Mining);
	if (!TestNotNull(TEXT("Miner at the mine"), AtMine))
	{
		return false;
	}
	const int32 MinerId = AtMine->Id;

	const FCytraOrderResult Clear = Match->Order(Rubble, FCytraOrderAction::Work(TEXT("Clear")));
	TestTrue(TEXT("Clear accepted"), Clear.IsAccepted());
	TestFalse(TEXT("Not waiting"), Clear.bWaiting);
	TestEqual(TEXT("The miner leaves the mine"), Match->FindJobAt(Rubble)->Worker, MinerId);
	TestTrue(TEXT("On the way"), Match->FindWorker(MinerId)->State == ECytraWorkerState::Move);
	TestEqual(TEXT("Mine empty"), Match->MinersAtMine(), 0);

	// No income while away; the time spent at the mine before is lost, as in the prototype.
	const int32 MinedBefore = Match->GetWallet().Mined;
	if (!TestTrue(TEXT("Clearing done"), StepUntilIdle(*Match)))
	{
		return false;
	}
	TestEqual(TEXT("Nothing mined while clearing"), Match->GetWallet().Mined, MinedBefore);
	TestTrue(TEXT("Heads back to the mine"), Match->FindWorker(MinerId)->State == ECytraWorkerState::ToMine);
	StepFor(*Match, 20.0);
	TestTrue(TEXT("Back at the mine"), Match->FindWorker(MinerId)->State == ECytraWorkerState::Mining);
	return true;
}

}

#endif
