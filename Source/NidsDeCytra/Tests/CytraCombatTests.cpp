// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraCombatTest
{
	constexpr double Dt = 1.0 / 60.0;
	constexpr double Tolerance = 1e-9;

	const FName Carpenter(TEXT("Carpenter"));
	const FName Engineer(TEXT("Engineer"));

	/** Next to the start of the south route, which wave 2 takes. */
	const FIntPoint SouthSlot(2, 12);

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

	/** Orders on Cell and steps until no job is left. */
	bool OrderAndWait(FAutomationTestBase& Test, FCytraMatch& Match, const FIntPoint& Cell, const FCytraOrderAction& Action)
	{
		if (!Test.TestTrue(TEXT("Order accepted"), Match.Order(Cell, Action).IsAccepted()))
		{
			return false;
		}
		for (int32 i = 0; i < 60 * 60 && !Match.GetJobs().IsEmpty(); ++i)
		{
			Match.Step(Dt);
		}
		return Test.TestTrue(TEXT("Job done"), Match.GetJobs().IsEmpty());
	}

	/** Builds Tower on the south slot, clears wave 1 by hand, then launches wave 2 (bandits on the south route). */
	bool StartWave2With(FAutomationTestBase& Test, FCytraMatch& Match, FName Tower)
	{
		if (!OrderAndWait(Test, Match, SouthSlot, FCytraOrderAction::Build(Tower)))
		{
			return false;
		}
		Match.LaunchWave();
		for (int32 i = 0; i < 60 * 60 && Match.GetPhase() != ECytraMatchPhase::Prep; ++i)
		{
			for (const FCytraEnemy& Enemy : Match.GetEnemies())
			{
				Match.DamageEnemy(Enemy.Id, 1e6);
			}
			Match.Step(Dt);
		}
		Match.ConsumeEffects();
		return Test.TestTrue(TEXT("Wave 1 cleared"), Match.GetPhase() == ECytraMatchPhase::Prep)
			&& Test.TestTrue(TEXT("Wave 2 launched"), Match.LaunchWave().bLaunched);
	}

	const FCytraEnemy* FindEnemy(const FCytraMatch& Match, int32 Id)
	{
		return Match.GetEnemies().FindByPredicate([Id](const FCytraEnemy& Enemy) { return Enemy.Id == Id; });
	}

	/** The enemy a tower on Cell should aim at: alive, on the map, in range, and the most advanced. */
	const FCytraEnemy* ExpectedTarget(const FCytraMatch& Match, const FIntPoint& Cell, double Range)
	{
		const FCytraEnemy* Best = nullptr;
		for (const FCytraEnemy& Enemy : Match.GetEnemies())
		{
			const bool bTargetable = !Enemy.bDead && Enemy.Position.X >= Match.GetBalance().TargetingMinX
				&& FVector2d::Distance(Enemy.Position, FVector2d(Cell.X, Cell.Y)) <= Range;
			if (bTargetable && (!Best || Enemy.Distance > Best->Distance))
			{
				Best = &Enemy;
			}
		}
		return Best;
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCombatBallistaTest, "Cytra.Rules.Combat.Ballista", CytraTest::Flags)

bool FCytraCombatBallistaTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this, { Carpenter });
	if (!Match || !StartWave2With(*this, *Match, TEXT("Ballista")))
	{
		return false;
	}
	const FCytraBalance& Balance = Match->GetBalance();
	const double Range = Balance.TowerStats(TEXT("Ballista"), 1, false).Range;
	const int32 BanditHp = Balance.EnemyHp(TEXT("Bandit"), 2);

	// Steps of the first shots, the target of the first one and the lead enemy's x just before it.
	TArray<int32> ShotSteps;
	int32 FirstTarget = 0;
	double LeadXBefore = 0.0;
	double LeadX = -100.0;
	for (int32 StepIndex = 1; StepIndex <= 60 * 10 && ShotSteps.Num() < 3; ++StepIndex)
	{
		const int32 ShotsBefore = Match->GetShots().Num();
		const double LeadXPrevious = LeadX;
		Match->Step(Dt);
		LeadX = Match->GetEnemies().IsEmpty() ? -100.0 : Match->GetEnemies()[0].Position.X;
		if (Match->GetShots().Num() > ShotsBefore)
		{
			const FCytraShot& Shot = Match->GetShots().Last();
			if (ShotSteps.IsEmpty())
			{
				FirstTarget = Shot.Target;
				LeadXBefore = LeadXPrevious;
				const FCytraEnemy* Expected = ExpectedTarget(*Match, SouthSlot, Range);
				TestTrue(TEXT("Aims at the most advanced enemy in range"), Expected && Expected->Id == Shot.Target);
				TestTrue(TEXT("Bolt"), Shot.Kind == ECytraShotKind::Bolt);
				TestEqual(TEXT("Bolt damage"), Shot.Damage, 15.0, Tolerance);
				TestEqual(TEXT("Bolt speed"), Shot.Speed, 14.0, Tolerance);
			}
			ShotSteps.Add(StepIndex);
		}
	}
	if (!TestEqual(TEXT("Three shots"), ShotSteps.Num(), 3))
	{
		return false;
	}

	// The first bandit comes into range while still off the map: the tower waits for x >= -0.4.
	const FCytraEnemy* First = FindEnemy(*Match, FirstTarget);
	TestTrue(TEXT("Lead enemy was off the map the step before"), LeadXBefore < Balance.TargetingMinX);
	// It reaches x = -0.4 after 0.6 cells, at 1.25 cells per second: 29 steps.
	TestEqual(TEXT("First shot step"), ShotSteps[0], 29);
	// Reload of 1 / 1.1 s, 54.5 steps: the next shot comes 55 steps later.
	TestEqual(TEXT("Reload"), ShotSteps[1] - ShotSteps[0], 55);
	TestEqual(TEXT("Steady rate"), ShotSteps[2] - ShotSteps[1], 55);
	// Two bolts have hit the first bandit by now.
	if (TestNotNull(TEXT("First target alive"), First))
	{
		TestEqual(TEXT("Hit points after two bolts"), First->Hp, BanditHp - 30.0, Tolerance);
	}

	// A bolt whose target dies before the impact vanishes without damage.
	const FCytraShot Flying = Match->GetShots().Last();
	const int32 GoldBefore = Match->GetWallet().Gold;
	Match->DamageEnemy(Flying.Target, 1e6);
	const int32 ShotsBefore = Match->GetShots().Num();
	Match->Step(Dt);
	TestEqual(TEXT("Lost bolt removed"), Match->GetShots().Num(), ShotsBefore - 1);
	TestEqual(TEXT("Only the kill was paid"), Match->GetWallet().Gold, GoldBefore + Balance.Enemies.FindChecked(TEXT("Bandit")).Gold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCombatMortarTest, "Cytra.Rules.Combat.Mortar", CytraTest::Flags)

bool FCytraCombatMortarTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this, { Carpenter });
	if (!Match || !StartWave2With(*this, *Match, TEXT("Mortar")))
	{
		return false;
	}
	const FCytraTowerDef& Def = Match->GetBalance().Towers.FindChecked(TEXT("Mortar"));

	// Wait for the first shell.
	for (int32 i = 0; i < 60 * 10 && Match->GetShots().IsEmpty(); ++i)
	{
		Match->Step(Dt);
	}
	if (!TestEqual(TEXT("One shell"), Match->GetShots().Num(), 1))
	{
		return false;
	}
	const FCytraShot Shell = Match->GetShots()[0];
	TestTrue(TEXT("Shell"), Shell.Kind == ECytraShotKind::Shell);
	TestEqual(TEXT("Shell damage"), Shell.Damage, 26.0, Tolerance);
	TestEqual(TEXT("Flight time"), Shell.Duration, 0.7, Tolerance);

	// Aimed 0.7 s ahead of the most advanced bandit, at its speed, along its route.
	const FCytraEnemy* Target = ExpectedTarget(*Match, SouthSlot, Match->GetBalance().TowerStats(TEXT("Mortar"), 1, false).Range);
	if (TestNotNull(TEXT("Target"), Target))
	{
		const double Ahead = Target->Speed * Def.FlightTime;
		const int32 TargetId = Target->Id;
		// Bandits walk a straight line at the start of the south route: the lead is along +X.
		TestEqual(TEXT("Aim x"), Shell.To.X, Target->Position.X + Ahead, 1e-6);
		TestEqual(TEXT("Aim y"), Shell.To.Y, Target->Position.Y, 1e-6);

		// Step until it lands, and check the splash on every enemy.
		TArray<TPair<int32, double>> HpBefore;
		while (!Match->GetShots().IsEmpty())
		{
			HpBefore.Reset();
			for (const FCytraEnemy& Enemy : Match->GetEnemies())
			{
				HpBefore.Emplace(Enemy.Id, Enemy.Hp);
			}
			Match->Step(Dt);
		}
		TArray<FCytraEffect> Effects = Match->ConsumeEffects();
		if (TestEqual(TEXT("One blast"), Effects.Num(), 1))
		{
			TestTrue(TEXT("Blast"), Effects[0].Kind == ECytraEffectKind::Blast);
			TestEqual(TEXT("Blast radius"), Effects[0].Radius, 1.15, Tolerance);
		}
		int32 Hits = 0;
		for (const TPair<int32, double>& Before : HpBefore)
		{
			const FCytraEnemy* Enemy = FindEnemy(*Match, Before.Key);
			if (!Enemy)
			{
				continue;
			}
			const bool bInBlast = FVector2d::Distance(Enemy->Position, Shell.To) <= Def.Splash;
			Hits += bInBlast ? 1 : 0;
			TestEqual(*FString::Printf(TEXT("Hit points of enemy %d"), Before.Key), Enemy->Hp, Before.Value - (bInBlast ? 26.0 : 0.0), Tolerance);
		}
		TestTrue(TEXT("The aimed bandit is in the blast"), FindEnemy(*Match, TargetId) && FVector2d::Distance(FindEnemy(*Match, TargetId)->Position, Shell.To) <= Def.Splash);
		TestTrue(TEXT("At least one hit"), Hits >= 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCombatCoilTest, "Cytra.Rules.Combat.Coil", CytraTest::Flags)

bool FCytraCombatCoilTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this, { Carpenter, Engineer });
	if (!Match || !StartWave2With(*this, *Match, TEXT("Coil")))
	{
		return false;
	}
	const FCytraBalance& Balance = Match->GetBalance();
	const FCytraTowerDef& Def = Balance.Towers.FindChecked(TEXT("Coil"));
	const double Range = Balance.TowerStats(TEXT("Coil"), 1, false).Range;
	const int32 BanditHp = Balance.EnemyHp(TEXT("Bandit"), 2);

	// Wait until three bandits are in range, so that the first discharge after that has three targets.
	TArray<FCytraEffect> Zaps;
	for (int32 i = 0; i < 60 * 20; ++i)
	{
		Match->Step(Dt);
		Zaps = Match->ConsumeEffects();
		if (Zaps.Num() == Def.Targets)
		{
			break;
		}
		if (!TestTrue(TEXT("Never more than three targets"), Zaps.Num() <= Def.Targets))
		{
			return false;
		}
	}
	if (!TestEqual(TEXT("Three zaps"), Zaps.Num(), 3))
	{
		return false;
	}

	// The three most advanced enemies in range are hit and slowed.
	TArray<const FCytraEnemy*> InRange;
	for (const FCytraEnemy& Enemy : Match->GetEnemies())
	{
		if (Enemy.Position.X >= Balance.TargetingMinX && FVector2d::Distance(Enemy.Position, FVector2d(SouthSlot.X, SouthSlot.Y)) <= Range)
		{
			InRange.Add(&Enemy);
		}
	}
	InRange.StableSort([](const FCytraEnemy& A, const FCytraEnemy& B) { return A.Distance > B.Distance; });
	const double Now = Match->GetTime();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FCytraEnemy& Enemy = *InRange[Index];
		TestEqual(TEXT("Zap goes to the enemy"), Zaps[Index].To, Enemy.Position);
		TestTrue(TEXT("Slowed"), Enemy.IsSlowed(Now));
		TestEqual(TEXT("Slow lasts 1.6 s"), Enemy.SlowUntil, Now + Def.SlowTime, Tolerance);
		TestEqual(TEXT("Slow factor"), Enemy.SlowFactor, 0.5, Tolerance);
	}

	// A slowed bandit walks at half speed.
	const int32 LeadId = InRange[0]->Id;
	const double LeadDistance = InRange[0]->Distance;
	const double LeadHp = InRange[0]->Hp;
	Match->Step(Dt);
	TestEqual(TEXT("Half speed"), FindEnemy(*Match, LeadId)->Distance - LeadDistance, 1.25 * 0.5 * Dt, Tolerance);

	// The next discharge, 0.8 s later, restarts the slow without stacking it, and the tower keeps firing while upgraded.
	TestTrue(TEXT("Upgrade accepted"), Match->Order(SouthSlot, FCytraOrderAction::Upgrade()).IsAccepted());
	Match->ConsumeEffects();
	for (int32 i = 0; i < 60 * 2 && Match->ConsumeEffects().IsEmpty(); ++i)
	{
		Match->Step(Dt);
	}
	TestNotNull(TEXT("Upgrade still in progress"), Match->FindJobAt(SouthSlot));
	const FCytraEnemy* Lead = FindEnemy(*Match, LeadId);
	if (TestNotNull(TEXT("Lead alive"), Lead))
	{
		TestEqual(TEXT("Slow restarted"), Lead->SlowUntil, Match->GetTime() + Def.SlowTime, Tolerance);
		TestEqual(TEXT("Slow not stacked"), Lead->SlowFactor, 0.5, Tolerance);
		TestEqual(TEXT("One more zap of 5"), Lead->Hp, LeadHp - 5.0, Tolerance);
		TestTrue(TEXT("Hit by every discharge since it came in range"), FMath::IsNearlyZero(FMath::Fmod(BanditHp - Lead->Hp, 5.0)));
		const double Before = Lead->Distance;
		Match->Step(Dt);
		TestEqual(TEXT("Still half speed"), FindEnemy(*Match, LeadId)->Distance - Before, 1.25 * 0.5 * Dt, Tolerance);
	}
	return true;
}

}

#endif
