// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMath.h"
#include "Rules/CytraWaveSchedule.h"

// GDD section 13, case 6.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTowersUpgradeCostsTest, "Cytra.Rules.Towers.UpgradeCosts", CytraTest::Flags)

bool FCytraTowersUpgradeCostsTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	TestEqual(TEXT("Ballista 1 -> 2"), Balance.UpgradeCost(TEXT("Ballista"), 1), 35);
	TestEqual(TEXT("Ballista 2 -> 3"), Balance.UpgradeCost(TEXT("Ballista"), 2), 70);
	TestEqual(TEXT("Mortar 1 -> 2"), Balance.UpgradeCost(TEXT("Mortar"), 1), 65);
	TestEqual(TEXT("Mortar 2 -> 3"), Balance.UpgradeCost(TEXT("Mortar"), 2), 125);
	TestEqual(TEXT("Coil 1 -> 2"), Balance.UpgradeCost(TEXT("Coil"), 1), 55);
	TestEqual(TEXT("Coil 2 -> 3"), Balance.UpgradeCost(TEXT("Coil"), 2), 110);
	return true;
}

// GDD section 13, case 7 (the formula; the payment at the end of the job comes with the job queue).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTowersDismantleRefundTest, "Cytra.Rules.Towers.DismantleRefund", CytraTest::Flags)

bool FCytraTowersDismantleRefundTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	const int32 Invested = Balance.Towers.FindChecked(TEXT("Ballista")).Cost + Balance.UpgradeCost(TEXT("Ballista"), 1) + Balance.UpgradeCost(TEXT("Ballista"), 2);
	TestEqual(TEXT("Level 3 ballista invested"), Invested, 155);
	TestEqual(TEXT("Refund"), Balance.DismantleRefundOf(Invested), 116);
	TestEqual(TEXT("Refund of a level 1 mortar"), Balance.DismantleRefundOf(90), 68);
	return true;
}

// GDD section 13, case 10.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTowersRangeTest, "Cytra.Rules.Towers.Range", CytraTest::Flags)

bool FCytraTowersRangeTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	TestEqual(TEXT("Level 1 ballista on a high slot"), Balance.TowerStats(TEXT("Ballista"), 1, true).Range, 3.64, 1e-9);
	TestEqual(TEXT("Level 3 ballista on a plain slot"), Balance.TowerStats(TEXT("Ballista"), 3, false).Range, 3.248, 1e-9);
	TestEqual(TEXT("Level 1 ballista on a plain slot"), Balance.TowerStats(TEXT("Ballista"), 1, false).Range, 2.8, 1e-9);
	TestEqual(TEXT("Level 3 ballista damage"), Balance.TowerStats(TEXT("Ballista"), 3, false).Damage, 33.0, 1e-9);
	TestEqual(TEXT("Level 2 mortar damage"), Balance.TowerStats(TEXT("Mortar"), 2, false).Damage, 41.6, 1e-9);
	TestEqual(TEXT("Rate does not change with level"), Balance.TowerStats(TEXT("Coil"), 3, true).Rate, 1.25);
	return true;
}

// GDD section 13, case 9.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraEnemiesHpScalingTest, "Cytra.Rules.Enemies.HpScaling", CytraTest::Flags)

bool FCytraEnemiesHpScalingTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	TestEqual(TEXT("Bandit, wave 1"), Balance.EnemyHp(TEXT("Bandit"), 1), 30);
	TestEqual(TEXT("Bandit, wave 6"), Balance.EnemyHp(TEXT("Bandit"), 6), 60);
	TestEqual(TEXT("Captain, wave 10"), Balance.EnemyHp(TEXT("Captain"), 10), 3500);
	TestEqual(TEXT("Adventurer, wave 4 (33.6 rounds to 34)"), Balance.EnemyHp(TEXT("Adventurer"), 4), 34);
	return true;
}

// GDD section 13, case 8, and the table of section 8.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraWavesThreatAndPrepTest, "Cytra.Rules.Waves.ThreatAndPrep", CytraTest::Flags)

bool FCytraWavesThreatAndPrepTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	if (!CytraTest::LoadAll(*this, Map, Balance, Waves) || !TestEqual(TEXT("Wave count"), Waves.Num(), 10))
	{
		return false;
	}

	const int32 Threats[] = { 240, 324, 588, 624, 1044, 1264, 1993, 2755, 3744, 7174 };
	const int32 Levels[] = { 1, 2, 2, 2, 2, 3, 3, 4, 4, 5 };
	const int32 PrepTimes[] = { 0, 24, 26, 26, 28, 28, 31, 32, 34, 40 };
	const int32 EndBonuses[] = { 31, 37, 43, 49, 55, 61, 67, 73, 79, 85 };
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const int32 Wave = Index + 1;
		TestEqual(FString::Printf(TEXT("Threat of wave %d"), Wave), CytraMath::Round(Waves.Threat(Index)), Threats[Index]);
		TestEqual(FString::Printf(TEXT("Threat level of wave %d"), Wave), Waves.ThreatLevel(Index, Balance), Levels[Index]);
		if (Index > 0)
		{
			// The first wave is launched by the player, without countdown.
			TestEqual(FString::Printf(TEXT("Preparation before wave %d"), Wave), Waves.PrepTime(Index, Balance), PrepTimes[Index]);
		}
		TestEqual(FString::Printf(TEXT("End bonus of wave %d"), Wave), Balance.WaveEndBonus(Wave), EndBonuses[Index]);
	}

	TestEqual(TEXT("Early launch, 23.7 s left"), Balance.EarlyLaunchBonus(23.7), 23);
	TestEqual(TEXT("Early launch, 0.4 s left"), Balance.EarlyLaunchBonus(0.4), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraWavesSpawnQueueTest, "Cytra.Rules.Waves.SpawnQueue", CytraTest::Flags)

bool FCytraWavesSpawnQueueTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	if (!CytraTest::LoadAll(*this, Map, Balance, Waves))
	{
		return false;
	}

	// Wave 3: 7 bandits north every second, 7 south every second after 2 s. Ties keep the data order: north first.
	const TArray<FCytraSpawn> Queue = Waves.SpawnQueue(2);
	if (TestEqual(TEXT("Wave 3 spawn count"), Queue.Num(), 14))
	{
		const double Times[] = { 0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 8 };
		const TCHAR* Routes[] = { TEXT("North"), TEXT("North"), TEXT("North"), TEXT("South"), TEXT("North"), TEXT("South"), TEXT("North"),
			TEXT("South"), TEXT("North"), TEXT("South"), TEXT("North"), TEXT("South"), TEXT("South"), TEXT("South") };
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			TestEqual(FString::Printf(TEXT("Spawn %d time"), i), Queue[i].Time, Times[i]);
			TestEqual(FString::Printf(TEXT("Spawn %d route"), i), Queue[i].Route, FName(Routes[i]));
		}
	}

	// Wave 6: the lone soldier comes last, after 8 s.
	const TArray<FCytraSpawn> Wave6 = Waves.SpawnQueue(5);
	if (TestEqual(TEXT("Wave 6 spawn count"), Wave6.Num(), 21))
	{
		TestEqual(TEXT("Wave 6 last enemy"), Wave6.Last().Enemy, FName(TEXT("Soldier")));
		TestEqual(TEXT("Wave 6 last time"), Wave6.Last().Time, 8.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraWavesForecastTest, "Cytra.Rules.Waves.Forecast", CytraTest::Flags)

bool FCytraWavesForecastTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	if (!CytraTest::LoadAll(*this, Map, Balance, Waves))
	{
		return false;
	}
	const TArray<FName> RouteOrder = { TEXT("North"), TEXT("South") };

	const TArray<FCytraRouteForecast> Wave1 = Waves.Forecast(0, RouteOrder);
	if (TestEqual(TEXT("Wave 1 uses one route"), Wave1.Num(), 1))
	{
		TestEqual(TEXT("Wave 1 route"), Wave1[0].Route, FName(TEXT("North")));
		TestEqual(TEXT("Wave 1 enemies"), Wave1[0].Enemies, TArray<TPair<FName, int32>>{ { TEXT("Bandit"), 8 } });
	}

	// Wave 6: north gets 8 bandits and a soldier, south 12 adventurers; north is announced first.
	const TArray<FCytraRouteForecast> Wave6 = Waves.Forecast(5, RouteOrder);
	if (TestEqual(TEXT("Wave 6 uses both routes"), Wave6.Num(), 2))
	{
		TestEqual(TEXT("Wave 6 first route"), Wave6[0].Route, FName(TEXT("North")));
		TestEqual(TEXT("Wave 6 north"), Wave6[0].Enemies, TArray<TPair<FName, int32>>{ { TEXT("Bandit"), 8 }, { TEXT("Soldier"), 1 } });
		TestEqual(TEXT("Wave 6 south"), Wave6[1].Enemies, TArray<TPair<FName, int32>>{ { TEXT("Adventurer"), 12 } });
	}
	return true;
}

#endif
