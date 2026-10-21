// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace
{
	/** The fixed step of the game and of the prototype. */
	constexpr double Dt = 1.0 / 60.0;

	/** Steps the match for about Seconds of game time. */
	void StepFor(FCytraMatch& Match, double Seconds)
	{
		const int32 Steps = FMath::RoundToInt32(Seconds / Dt);
		for (int32 i = 0; i < Steps; ++i)
		{
			Match.Step(Dt);
		}
	}

	void KillAll(FCytraMatch& Match)
	{
		TArray<int32> Ids;
		for (const FCytraEnemy& Enemy : Match.GetEnemies())
		{
			Ids.Add(Enemy.Id);
		}
		for (const int32 Id : Ids)
		{
			Match.DamageEnemy(Id, 1.0e9);
		}
	}

	/** Launches the next wave and kills every enemy as soon as it spawns, until the wave is cleared. */
	bool ClearWave(FCytraMatch& Match)
	{
		Match.LaunchWave();
		for (int32 Guard = 0; Guard < 60 * 120 && Match.GetPhase() == ECytraMatchPhase::Wave; ++Guard)
		{
			Match.Step(Dt);
			KillAll(Match);
		}
		return Match.GetPhase() != ECytraMatchPhase::Wave;
	}

	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test)
	{
		FCytraMapDef Map;
		FCytraBalance Balance;
		FCytraWaveSchedule Waves;
		if (!CytraTest::LoadAll(Test, Map, Balance, Waves))
		{
			return {};
		}
		return FCytraMatch(Map, Balance, Waves);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchStartStateTest, "Cytra.Rules.Match.StartState", CytraTest::Flags)

bool FCytraMatchStartStateTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	TestEqual(TEXT("Gold"), Match->GetWallet().Gold, 170);
	TestEqual(TEXT("Eggs"), Match->GetEggs(), 12);
	TestEqual(TEXT("Wave"), Match->GetWave(), 0);
	TestEqual(TEXT("Wave count"), Match->GetWaveCount(), 10);
	TestTrue(TEXT("Preparation"), Match->GetPhase() == ECytraMatchPhase::Prep);
	TestFalse(TEXT("No countdown before wave 1"), Match->GetCountdown().IsSet());

	// Without a countdown, nothing launches on its own.
	StepFor(*Match, 120.0);
	TestTrue(TEXT("Still in preparation after 2 minutes"), Match->GetPhase() == ECytraMatchPhase::Prep);
	TestEqual(TEXT("Wave after 2 minutes"), Match->GetWave(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchLaunchWaveTest, "Cytra.Rules.Match.LaunchWave", CytraTest::Flags)

bool FCytraMatchLaunchWaveTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraLaunchResult First = Match->LaunchWave();
	TestTrue(TEXT("Wave 1 launched"), First.bLaunched);
	TestEqual(TEXT("No bonus without a countdown"), First.Bonus, 0);
	TestEqual(TEXT("Gold"), Match->GetWallet().Gold, 170);
	TestEqual(TEXT("Wave"), Match->GetWave(), 1);
	TestTrue(TEXT("Wave phase"), Match->GetPhase() == ECytraMatchPhase::Wave);
	TestEqual(TEXT("Pending spawns"), Match->GetPendingSpawns(), 8);
	TestFalse(TEXT("Second launch during the wave refused"), Match->LaunchWave().bLaunched);

	Match->Step(Dt);
	TestEqual(TEXT("First bandit spawned on the first step"), Match->GetEnemies().Num(), 1);
	TestEqual(TEXT("Pending spawns after one step"), Match->GetPendingSpawns(), 7);
	if (Match->GetEnemies().Num() == 1)
	{
		const FCytraEnemy& Bandit = Match->GetEnemies()[0];
		TestEqual(TEXT("Type"), Bandit.Type, FName(TEXT("Bandit")));
		TestEqual(TEXT("Route"), Match->GetMap().Routes[Bandit.Route].Id, FName(TEXT("North")));
		TestEqual(TEXT("Hit points"), Bandit.MaxHp, 30);
		TestEqual(TEXT("Moved during its first step"), Bandit.Distance, 1.25 * Dt, 1e-12);
	}

	// Second bandit 1.1 s after the first.
	StepFor(*Match, 1.0);
	TestEqual(TEXT("Enemies at 1.0 s"), Match->GetEnemies().Num(), 1);
	StepFor(*Match, 0.2);
	TestEqual(TEXT("Enemies at 1.2 s"), Match->GetEnemies().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchLeakTest, "Cytra.Rules.Match.Leak", CytraTest::Flags)

bool FCytraMatchLeakTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	// 27 cells at 1.25 cells per second: the first bandit reaches the nest after 21.6 s.
	Match->LaunchWave();
	StepFor(*Match, 21.5);
	TestEqual(TEXT("Eggs at 21.5 s"), Match->GetEggs(), 12);
	Match->ConsumeEvents();
	StepFor(*Match, 0.2);
	TestEqual(TEXT("Eggs at 21.7 s"), Match->GetEggs(), 11);
	TestEqual(TEXT("No gold for a leak"), Match->GetWallet().Gold, 170);
	TestEqual(TEXT("No kill for a leak"), Match->GetKills(), 0);
	TestEqual(TEXT("Enemies left on the map"), Match->GetEnemies().Num(), 7);

	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	if (TestEqual(TEXT("One event"), Events.Num(), 1))
	{
		TestTrue(TEXT("Leak event"), Events[0].Type == ECytraMatchEventType::Leak);
		TestEqual(TEXT("Leak enemy"), Events[0].Enemy, FName(TEXT("Bandit")));
		TestEqual(TEXT("Leak eggs"), Events[0].Eggs, 1);
	}
	TestEqual(TEXT("Events are consumed"), Match->ConsumeEvents().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchWaveClearedTest, "Cytra.Rules.Match.WaveCleared", CytraTest::Flags)

bool FCytraMatchWaveClearedTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	if (!TestTrue(TEXT("Wave 1 cleared"), ClearWave(*Match)))
	{
		return false;
	}
	TestTrue(TEXT("Back to preparation"), Match->GetPhase() == ECytraMatchPhase::Prep);
	TestEqual(TEXT("Kills"), Match->GetKills(), 8);
	TestEqual(TEXT("Gold: 170 + 8 bandits x 6 + bonus 31"), Match->GetWallet().Gold, 249);
	TestEqual(TEXT("Eggs"), Match->GetEggs(), 12);
	TestTrue(TEXT("Countdown set"), Match->GetCountdown().IsSet());
	TestEqual(TEXT("Countdown before wave 2"), Match->GetCountdown().Get(0.0), 24.0);

	const TArray<FCytraMatchEvent> Events = Match->ConsumeEvents();
	if (TestEqual(TEXT("One event"), Events.Num(), 1))
	{
		TestTrue(TEXT("Wave cleared event"), Events[0].Type == ECytraMatchEventType::WaveCleared);
		TestEqual(TEXT("Event wave"), Events[0].Wave, 1);
		TestEqual(TEXT("Event bonus"), Events[0].Gold, 31);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchAutoLaunchTest, "Cytra.Rules.Match.AutoLaunch", CytraTest::Flags)

bool FCytraMatchAutoLaunchTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match || !ClearWave(*Match))
	{
		return false;
	}

	const int32 Gold = Match->GetWallet().Gold;
	StepFor(*Match, 23.9);
	TestTrue(TEXT("Still in preparation at 23.9 s"), Match->GetPhase() == ECytraMatchPhase::Prep);
	StepFor(*Match, 0.2);
	TestTrue(TEXT("Wave 2 launched at 24 s"), Match->GetPhase() == ECytraMatchPhase::Wave);
	TestEqual(TEXT("Wave"), Match->GetWave(), 2);
	TestFalse(TEXT("Countdown cleared"), Match->GetCountdown().IsSet());
	TestEqual(TEXT("No bonus for an automatic launch"), Match->GetWallet().Gold, Gold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchEarlyLaunchTest, "Cytra.Rules.Match.EarlyLaunch", CytraTest::Flags)

bool FCytraMatchEarlyLaunchTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match || !ClearWave(*Match))
	{
		return false;
	}

	const int32 Gold = Match->GetWallet().Gold;
	StepFor(*Match, 5.3);
	TestEqual(TEXT("Countdown left"), Match->GetCountdown().Get(0.0), 18.7, 1e-6);
	const FCytraLaunchResult Launch = Match->LaunchWave();
	TestTrue(TEXT("Launched"), Launch.bLaunched);
	TestEqual(TEXT("Bonus for 18.7 s left"), Launch.Bonus, 18);
	TestEqual(TEXT("Gold"), Match->GetWallet().Gold, Gold + 18);
	TestEqual(TEXT("Bonus not counted as spent"), Match->GetWallet().Spent, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchDefeatTest, "Cytra.Rules.Match.DefeatWithoutDefense", CytraTest::Flags)

bool FCytraMatchDefeatTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	// Every enemy reaches the nest: wave 1 costs 8 eggs, wave 2 takes the last 4.
	Match->LaunchWave();
	int32 EggsAfterWave1 = INDEX_NONE;
	for (int32 Guard = 0; Guard < 60 * 300 && !Match->IsOver(); ++Guard)
	{
		Match->Step(Dt);
		if (EggsAfterWave1 == INDEX_NONE && Match->GetPhase() == ECytraMatchPhase::Prep)
		{
			EggsAfterWave1 = Match->GetEggs();
		}
	}

	TestEqual(TEXT("Eggs after wave 1"), EggsAfterWave1, 4);
	TestTrue(TEXT("Lost"), Match->GetPhase() == ECytraMatchPhase::Lost);
	TestEqual(TEXT("Lost during wave 2"), Match->GetWave(), 2);
	TestEqual(TEXT("Eggs"), Match->GetEggs(), 0);
	TestEqual(TEXT("Gold: 170 + bonus 31"), Match->GetWallet().Gold, 201);
	TestEqual(TEXT("Kills"), Match->GetKills(), 0);

	const double Time = Match->GetTime();
	const int32 Enemies = Match->GetEnemies().Num();
	StepFor(*Match, 10.0);
	TestEqual(TEXT("The match is frozen"), Match->GetTime(), Time);
	TestEqual(TEXT("Enemies frozen"), Match->GetEnemies().Num(), Enemies);
	TestFalse(TEXT("Launch refused once lost"), Match->LaunchWave().bLaunched);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchVictoryTest, "Cytra.Rules.Match.Victory", CytraTest::Flags)

bool FCytraMatchVictoryTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	const FCytraBalance& Balance = Match->GetBalance();
	const FCytraWaveSchedule& Waves = Match->GetWaves();
	int32 Enemies = 0;
	int32 KillGold = 0;
	for (const TArray<FCytraWaveGroup>& Wave : Waves.Waves)
	{
		for (const FCytraWaveGroup& Group : Wave)
		{
			Enemies += Group.Count;
			KillGold += Group.Count * Balance.Enemies.FindChecked(Group.Enemy).Gold;
		}
	}

	// Each wave is launched as soon as the previous one is cleared: the bonus is the whole countdown.
	int32 EarlyBonus = 0;
	bool bHpChecked = true;
	for (int32 Wave = 1; Wave <= Waves.Num(); ++Wave)
	{
		if (Wave > 1)
		{
			EarlyBonus += Waves.PrepTime(Wave - 1, Balance);
		}
		Match->LaunchWave();
		for (int32 Guard = 0; Guard < 60 * 120 && Match->GetPhase() == ECytraMatchPhase::Wave; ++Guard)
		{
			Match->Step(Dt);
			for (const FCytraEnemy& Enemy : Match->GetEnemies())
			{
				bHpChecked &= Enemy.MaxHp == Balance.EnemyHp(Enemy.Type, Wave);
			}
			KillAll(*Match);
		}
		if (Wave < Waves.Num() && !TestTrue(FString::Printf(TEXT("Wave %d cleared"), Wave), Match->GetPhase() == ECytraMatchPhase::Prep))
		{
			return false;
		}
	}

	TestTrue(TEXT("Hit points scaled by wave"), bHpChecked);
	TestEqual(TEXT("Early bonus total"), EarlyBonus, 269);
	TestTrue(TEXT("Won"), Match->GetPhase() == ECytraMatchPhase::Won);
	TestEqual(TEXT("Wave"), Match->GetWave(), 10);
	TestEqual(TEXT("Eggs"), Match->GetEggs(), 12);
	TestEqual(TEXT("Kills"), Match->GetKills(), Enemies);
	TestFalse(TEXT("No countdown after the last wave"), Match->GetCountdown().IsSet());
	// End bonuses: 25 + 6n for n = 1..10 = 580.
	TestEqual(TEXT("Gold"), Match->GetWallet().Gold, 170 + KillGold + 580 + EarlyBonus);
	TestFalse(TEXT("Launch refused once won"), Match->LaunchWave().bLaunched);
	return true;
}

#endif
