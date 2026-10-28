// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraFullGameTest
{
	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 StepsPerSecond = 60;
	/** A checkpoint every 30 seconds of play, and one at the end. */
	constexpr int32 CheckpointSteps = 30 * StepsPerSecond;
	constexpr int32 MaxSeconds = 1800;

	/** An order or a recruitment of the scripted player, tried from Seconds on. */
	struct FEntry
	{
		int32 Seconds = 0;
		bool bRecruit = false;
		FIntPoint Cell = FIntPoint::ZeroValue;
		FCytraOrderAction Action;
		FName Role;

		static FEntry Order(int32 Seconds, int32 X, int32 Y, const FCytraOrderAction& Action)
		{
			return FEntry{ Seconds, false, FIntPoint(X, Y), Action, NAME_None };
		}

		static FEntry Recruit(int32 Seconds, const TCHAR* Role)
		{
			return FEntry{ Seconds, true, FIntPoint::ZeroValue, FCytraOrderAction(), FName(Role) };
		}
	};

	/**
	 * The scripted player. Before every step, the queue is tried in order: an entry refused for lack of gold
	 * makes the queue wait, an entry refused for any other reason is dropped. The waves are launched at fixed times.
	 */
	TArray<FEntry> MakeQueue()
	{
		return {
			FEntry::Order(0, 19, 6, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 11, 4, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 14, 11, FCytraOrderAction::Work(TEXT("OpenMine"))),
			FEntry::Order(0, 10, 10, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 2, 12, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 3, 2, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Recruit(0, TEXT("Miner")),
			FEntry::Order(0, 13, 8, FCytraOrderAction::Work(TEXT("Clear"))),
			FEntry::Order(0, 14, 7, FCytraOrderAction::Work(TEXT("Ladder"))),
			FEntry::Order(0, 15, 7, FCytraOrderAction::Build(TEXT("Mortar"))),
			FEntry::Recruit(0, TEXT("Engineer")),
			FEntry::Order(0, 19, 6, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 9, 6, FCytraOrderAction::Work(TEXT("Bridge"))),
			FEntry::Order(0, 10, 7, FCytraOrderAction::Build(TEXT("Coil"))),
			FEntry::Order(0, 15, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 10, 10, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 11, 4, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 8, 7, FCytraOrderAction::Build(TEXT("Mortar"))),
			FEntry::Order(0, 13, 7, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 4, 7, FCytraOrderAction::Work(TEXT("Outpost"))),
			FEntry::Recruit(0, TEXT("Carpenter")),
			FEntry::Order(0, 19, 6, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 15, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(250, 3, 2, FCytraOrderAction::Dismantle()),
			FEntry::Order(0, 3, 2, FCytraOrderAction::Build(TEXT("Mortar"))),
			FEntry::Order(0, 2, 10, FCytraOrderAction::Work(TEXT("Clear"))),
			FEntry::Order(0, 2, 9, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 2, 12, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 13, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 10, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 7, 3, FCytraOrderAction::Work(TEXT("Ladder"))),
			FEntry::Order(0, 8, 3, FCytraOrderAction::Build(TEXT("Coil"))),
			FEntry::Order(0, 7, 10, FCytraOrderAction::Work(TEXT("Bridge"))),
			FEntry::Order(0, 6, 11, FCytraOrderAction::Build(TEXT("Mortar"))),
			FEntry::Order(0, 8, 11, FCytraOrderAction::Build(TEXT("Ballista"))),
			FEntry::Order(0, 8, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 10, 10, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 11, 4, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 8, 3, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 6, 11, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 13, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 2, 9, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 10, 7, FCytraOrderAction::Upgrade()),
			FEntry::Order(0, 8, 7, FCytraOrderAction::Upgrade()),
		};
	}

	const int32 Launches[] = { 25, 300, 500, 700 };

	/**
	 * The same match played by the logic of the web prototype (Docs/prototype/nids-de-cytra.html) in a headless browser:
	 * step, gold, eggs, wave, phase, kills, gold spent, gold mined, enemies alive, towers, workers, jobs, queue entries left.
	 */
	const TCHAR* const Reference[] = {
		TEXT("0,20,12,0,prep,0,150,0,0,0,2,3,41"),
		TEXT("1800,26,12,1,wave,0,150,6,5,2,2,0,41"),
		TEXT("3600,15,12,1,prep,8,250,16,0,4,2,0,39"),
		TEXT("5400,11,12,2,wave,14,300,26,3,4,2,1,38"),
		TEXT("7200,8,12,2,prep,17,370,38,0,5,3,0,34"),
		TEXT("9000,0,12,3,prep,31,525,58,0,5,4,1,29"),
		TEXT("10800,15,12,4,wave,36,560,78,11,5,4,1,28"),
		TEXT("12600,25,12,4,prep,47,685,98,0,6,4,0,26"),
		TEXT("14400,3,12,5,wave,58,805,118,1,7,4,1,24"),
		TEXT("16200,46,12,5,prep,59,855,138,0,7,5,0,23"),
		TEXT("18000,50,12,7,wave,80,1130,152,0,6,5,3,9"),
		TEXT("19800,9,12,7,wave,100,1335,172,1,6,5,2,4"),
		TEXT("21600,44,12,7,prep,101,1405,192,0,6,5,0,1"),
		TEXT("23400,107,6,8,wave,125,1530,212,2,6,5,0,0"),
		TEXT("25200,236,6,8,prep,127,1530,232,0,6,5,0,0"),
		TEXT("27000,358,6,9,wave,140,1530,252,13,6,5,0,0"),
		TEXT("27861,420,0,9,lost,149,1530,260,0,6,5,0,0"),
	};

	const TCHAR* PhaseName(ECytraMatchPhase Phase)
	{
		switch (Phase)
		{
		case ECytraMatchPhase::Prep:
			return TEXT("prep");
		case ECytraMatchPhase::Wave:
			return TEXT("wave");
		case ECytraMatchPhase::Won:
			return TEXT("won");
		case ECytraMatchPhase::Lost:
			return TEXT("lost");
		}
		return TEXT("?");
	}

	FString Snapshot(int32 Step, const FCytraMatch& Match, int32 QueueLeft)
	{
		const int32 Alive = Match.GetEnemies().FilterByPredicate([](const FCytraEnemy& Enemy) { return !Enemy.bDead; }).Num();
		const FCytraWallet& Wallet = Match.GetWallet();
		return FString::Printf(TEXT("%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d"), Step, Wallet.Gold, Match.GetEggs(), Match.GetWave(),
			PhaseName(Match.GetPhase()), Match.GetKills(), Wallet.Spent, Wallet.Mined, Alive, Match.GetTowers().Num(),
			Match.GetWorkers().Num(), Match.GetJobs().Num(), QueueLeft);
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraFullGameTest, "Cytra.Rules.Match.FullGameVsPrototype", CytraTest::Flags)

bool FCytraFullGameTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	if (!CytraTest::LoadAll(*this, Map, Balance, Waves))
	{
		return false;
	}
	FCytraMatch Match(Map, Balance, Waves);
	TArray<FEntry> Queue = MakeQueue();

	// Same loop as the browser harness. The end is checked before the queue: once the match is over,
	// the prototype refuses recruits and the GDD does not, and that difference must not show here.
	TArray<FString> Snapshots;
	int32 NextLaunch = 0;
	for (int32 Step = 0; Step <= MaxSeconds * StepsPerSecond; ++Step)
	{
		if (Match.IsOver())
		{
			Snapshots.Add(Snapshot(Step, Match, Queue.Num()));
			break;
		}
		while (NextLaunch < UE_ARRAY_COUNT(Launches) && Launches[NextLaunch] * StepsPerSecond <= Step)
		{
			Match.LaunchWave();
			++NextLaunch;
		}
		while (!Queue.IsEmpty() && Queue[0].Seconds * StepsPerSecond <= Step)
		{
			const FEntry& Entry = Queue[0];
			const bool bShortOfGold = Entry.bRecruit
				? Match.Recruit(Entry.Role).Refusal == ECytraRecruitRefusal::NotEnoughGold
				: Match.Order(Entry.Cell, Entry.Action).Check.Refusal == ECytraOrderRefusal::NotEnoughGold;
			if (bShortOfGold)
			{
				break;
			}
			Queue.RemoveAt(0);
		}
		if (Step % CheckpointSteps == 0)
		{
			Snapshots.Add(Snapshot(Step, Match, Queue.Num()));
		}
		Match.Step(Dt);
	}

	const int32 Count = UE_ARRAY_COUNT(Reference);
	for (int32 Index = 0; Index < FMath::Min(Count, Snapshots.Num()); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Checkpoint %d"), Index), Snapshots[Index], FString(Reference[Index]));
	}
	TestEqual(TEXT("Same number of checkpoints"), Snapshots.Num(), Count);
	return true;
}

} // namespace CytraFullGameTest

#endif
