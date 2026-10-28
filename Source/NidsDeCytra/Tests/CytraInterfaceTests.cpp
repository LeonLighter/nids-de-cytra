// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"
#include "UI/CytraHudView.h"
#include "UI/CytraMessages.h"

namespace CytraInterfaceTest
{
	constexpr double Dt = 1.0 / 60.0;

	const FName Carpenter(TEXT("Carpenter"));
	const FName Miner(TEXT("Miner"));
	const FName Engineer(TEXT("Engineer"));

	/** A match on the real data, optionally with another starting crew, fewer eggs or fewer waves. */
	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test, const TArray<FName>* StartRoles = nullptr, int32 StartEggs = 0, int32 WaveCount = 0)
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
		if (StartEggs > 0)
		{
			Balance.StartEggs = StartEggs;
		}
		if (WaveCount > 0)
		{
			Waves.Waves.SetNum(WaveCount);
			Waves.ComputeThreats(Balance);
		}
		return FCytraMatch(Map, Balance, Waves);
	}

	/** Steps until an event matching Predicate comes, for at most a minute. Returns it, or nothing. */
	TOptional<FCytraMatchEvent> StepUntilEvent(FCytraMatch& Match, TFunctionRef<bool(const FCytraMatchEvent&)> Predicate)
	{
		for (int32 i = 0; i < 60 * 60; ++i)
		{
			Match.Step(Dt);
			for (const FCytraMatchEvent& Event : Match.ConsumeEvents())
			{
				if (Predicate(Event))
				{
					return Event;
				}
			}
		}
		return {};
	}

	bool StepUntilIdle(FCytraMatch& Match)
	{
		for (int32 i = 0; i < 60 * 60 && !Match.GetJobs().IsEmpty(); ++i)
		{
			Match.Step(Dt);
		}
		return Match.GetJobs().IsEmpty();
	}

	FString EventText(const FCytraMatchEvent& Event)
	{
		const TOptional<FCytraMessage> Message = FCytraMessages::FromEvent(Event);
		return Message ? Message->Text.ToString() : FString(TEXT("<no message>"));
	}

	FCytraMatchEvent JobDone(ECytraOrderKind Kind, const TCHAR* Work)
	{
		FCytraMatchEvent Event;
		Event.Type = ECytraMatchEventType::JobDone;
		Event.Job = Kind;
		Event.Work = Work;
		return Event;
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMessagesEventsTest, "Cytra.UI.Messages.Events", CytraTest::Flags)

bool FCytraMessagesEventsTest::RunTest(const FString& Parameters)
{
	// Access opened, with the slots it gave, as in the prototype.
	FCytraMatchEvent Clear = JobDone(ECytraOrderKind::Work, TEXT("Clear"));
	Clear.OpenedSlots = 1;
	TestEqual(TEXT("Rubble cleared"), EventText(Clear), FString(TEXT("Éboulis déblayé : 1 emplacement accessible")));
	FCytraMatchEvent Bridge = JobDone(ECytraOrderKind::Work, TEXT("Bridge"));
	Bridge.OpenedSlots = 2;
	TestEqual(TEXT("Bridge built"), EventText(Bridge), FString(TEXT("Pont terminé : 2 emplacements accessibles")));
	TestEqual(TEXT("Ladder without new slot"), EventText(JobDone(ECytraOrderKind::Work, TEXT("Ladder"))), FString(TEXT("Échelle posée")));
	TestEqual(TEXT("Mine opened"), EventText(JobDone(ECytraOrderKind::Work, TEXT("OpenMine"))), FString(TEXT("Mine ouverte : les mineurs sans chantier y descendent")));
	TestEqual(TEXT("Outpost built"), EventText(JobDone(ECytraOrderKind::Work, TEXT("Outpost"))), FString(TEXT("Avant-poste monté : les ouvriers rentrent au camp le plus proche")));

	// Towers.
	TestEqual(TEXT("Tower built"), EventText(JobDone(ECytraOrderKind::Build, TEXT("Coil"))), FString(TEXT("Bobine gnome en service")));
	FCytraMatchEvent Upgraded = JobDone(ECytraOrderKind::Upgrade, TEXT("Mortar"));
	Upgraded.Level = 3;
	TestEqual(TEXT("Tower upgraded"), EventText(Upgraded), FString(TEXT("Mortier au niveau 3")));
	FCytraMatchEvent Dismantled = JobDone(ECytraOrderKind::Dismantle, TEXT("Ballista"));
	Dismantled.Refund = 116;
	TestEqual(TEXT("Tower dismantled"), EventText(Dismantled), FString(TEXT("Démontage terminé (Baliste) : +116 or")));

	// Eggs lost: a warning.
	FCytraMatchEvent Leak;
	Leak.Type = ECytraMatchEventType::Leak;
	Leak.Enemy = TEXT("Soldier");
	Leak.Eggs = 2;
	const TOptional<FCytraMessage> LeakMessage = FCytraMessages::FromEvent(Leak);
	if (TestTrue(TEXT("Leak message"), LeakMessage.IsSet()))
	{
		TestEqual(TEXT("Leak text"), LeakMessage->Text.ToString(), FString(TEXT("Un soldat d’Arch-Dawn a atteint le nid : −2 œufs")));
		TestTrue(TEXT("Leak is a warning"), LeakMessage->Tone == ECytraMessageTone::Warning);
	}
	Leak.Enemy = TEXT("Bandit");
	Leak.Eggs = 1;
	TestEqual(TEXT("One egg"), EventText(Leak), FString(TEXT("Un bandit a atteint le nid : −1 œuf")));

	FCytraMatchEvent Cleared;
	Cleared.Type = ECytraMatchEventType::WaveCleared;
	Cleared.Wave = 3;
	Cleared.Gold = 43;
	TestEqual(TEXT("Wave cleared"), EventText(Cleared), FString(TEXT("Vague 3 repoussée : +43 or")));
	TestTrue(TEXT("Wave cleared is good news"), FCytraMessages::FromEvent(Cleared)->Tone == ECytraMessageTone::Good);

	// Gold from the mine shows over the board, without a message.
	FCytraMatchEvent Yield;
	Yield.Type = ECytraMatchEventType::MineYield;
	Yield.Gold = 2;
	TestFalse(TEXT("No message for the mine"), FCytraMessages::FromEvent(Yield).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMessagesOrdersTest, "Cytra.UI.Messages.Orders", CytraTest::Flags)

bool FCytraMessagesOrdersTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Crew = { Carpenter, Miner, Engineer };
	TOptional<FCytraMatch> Match = MakeMatch(*this, &Crew);
	if (!Match)
	{
		return false;
	}
	const FIntPoint First(3, 2);
	const FIntPoint Second(11, 4);

	FCytraMessage Message = FCytraMessages::FromOrder(Match->Order(First, FCytraOrderAction::Build(TEXT("Ballista"))));
	TestEqual(TEXT("Order accepted"), Message.Text.ToString(), FString(TEXT("Baliste : −50 or")));
	TestTrue(TEXT("Accepted is good news"), Message.Tone == ECytraMessageTone::Good);
	Message = FCytraMessages::FromOrder(Match->Order(Second, FCytraOrderAction::Build(TEXT("Ballista"))));
	TestEqual(TEXT("Order waiting"), Message.Text.ToString(), FString(TEXT("Baliste : −50 or, en attente d’un charpentier libre")));
	Message = FCytraMessages::FromOrder(Match->Order(FIntPoint(2, 12), FCytraOrderAction::Build(TEXT("Mortar"))));
	TestEqual(TEXT("Order refused"), Message.Text.ToString(), FString(TEXT("Il manque 20 or")));
	TestTrue(TEXT("Refused is a warning"), Message.Tone == ECytraMessageTone::Warning);

	// The first ballista in service, then upgraded by the engineer.
	const TOptional<FCytraMatchEvent> Built = StepUntilEvent(*Match, [](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::JobDone; });
	if (!TestTrue(TEXT("Ballista built"), Built.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("Built message"), EventText(*Built), FString(TEXT("Baliste en service")));
	Message = FCytraMessages::FromOrder(Match->Order(Built->Cell, FCytraOrderAction::Upgrade()));
	TestEqual(TEXT("Upgrade ordered"), Message.Text.ToString(), FString(TEXT("Amélioration : −35 or")));
	if (!TestTrue(TEXT("All jobs done"), StepUntilIdle(*Match)))
	{
		return false;
	}

	// Dismantling gives back 75 % of 85 gold, at the end of the job.
	const FCytraOrderResult Dismantle = Match->Order(Built->Cell, FCytraOrderAction::Dismantle());
	TestEqual(TEXT("Dismantle ordered"), FCytraMessages::FromOrder(Dismantle).Text.ToString(), FString(TEXT("Démontage commandé : +64 or à la fin")));

	// Recruits and early launches.
	TestEqual(TEXT("Recruited"), FCytraMessages::FromRecruit(Engineer, FCytraRecruitCheck()).Text.ToString(), FString(TEXT("Gnome ingénieur recruté : il arrive au camp")));
	FCytraRecruitCheck Full;
	Full.Refusal = ECytraRecruitRefusal::CrewFull;
	Full.MaxCrew = 8;
	const FCytraMessage Refused = FCytraMessages::FromRecruit(Miner, Full);
	TestEqual(TEXT("Recruit refused"), Refused.Text.ToString(), FString(TEXT("Équipe complète (8)")));
	TestTrue(TEXT("Recruit refused is a warning"), Refused.Tone == ECytraMessageTone::Warning);

	FCytraLaunchResult Launch;
	Launch.bLaunched = true;
	TestFalse(TEXT("No message without a bonus"), FCytraMessages::FromLaunch(Launch).IsSet());
	Launch.Bonus = 18;
	const TOptional<FCytraMessage> Early = FCytraMessages::FromLaunch(Launch);
	TestEqual(TEXT("Early launch"), Early ? Early->Text.ToString() : FString(), FString(TEXT("Vague lancée en avance : +18 or")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraHudForecastTest, "Cytra.UI.Hud.Forecast", CytraTest::Flags)

bool FCytraHudForecastTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	TestEqual(TEXT("Wave 1"), FCytraHudView::Forecast(*Match, 0).ToString(), FString(TEXT("Nord : 8 bandits")));
	TestEqual(TEXT("Wave 2"), FCytraHudView::Forecast(*Match, 1).ToString(), FString(TEXT("Sud : 9 bandits")));
	TestEqual(TEXT("Wave 3"), FCytraHudView::Forecast(*Match, 2).ToString(), FString(TEXT("Nord : 7 bandits · Sud : 7 bandits")));
	// North first, as in the prototype, though the data lists the south group first.
	TestEqual(TEXT("Wave 6"), FCytraHudView::Forecast(*Match, 5).ToString(), FString(TEXT("Nord : 8 bandits, 1 soldat d’Arch-Dawn · Sud : 12 aventuriers")));
	TestTrue(TEXT("Nothing after the last wave"), FCytraHudView::Forecast(*Match, 10).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraEndScreenTest, "Cytra.UI.End.Screen", CytraTest::Flags)

bool FCytraEndScreenTest::RunTest(const FString& Parameters)
{
	// Lost: with a single egg, the first bandit to reach the nest ends the match.
	TOptional<FCytraMatch> Match = MakeMatch(*this, nullptr, 1);
	if (!Match)
	{
		return false;
	}
	TestFalse(TEXT("No end screen during the match"), FCytraEndView::Build(*Match).IsSet());
	Match->LaunchWave();
	for (int32 i = 0; i < 60 * 60 && !Match->IsOver(); ++i)
	{
		Match->Step(Dt);
	}
	TOptional<FCytraEndView> End = FCytraEndView::Build(*Match);
	if (TestTrue(TEXT("Lost"), End.IsSet()))
	{
		TestFalse(TEXT("Not won"), End->bWon);
		TestEqual(TEXT("Lost title"), End->Title.ToString(), FString(TEXT("La couvée est perdue")));
		TestEqual(TEXT("Lost text"), End->Body.ToString(), FString(TEXT("Le nid est tombé à la vague 1. Ouvre plus tôt la mine et les positions qui couvrent les deux passes.")));
		TestEqual(TEXT("Lost progress"), End->Progress.ToString(), FString(TEXT("Vague 1/10 · 0 œuf restant")));
		TestEqual(TEXT("Lost stats"), End->Stats.ToString(), FString(TEXT("0 assaillant abattu · 0 or dépensé · 0 or extrait · 0 tourelle")));
	}

	// Won: a single wave, every enemy killed as it appears.
	Match = MakeMatch(*this, nullptr, 0, 1);
	if (!Match)
	{
		return false;
	}
	Match->LaunchWave();
	for (int32 i = 0; i < 60 * 60 && !Match->IsOver(); ++i)
	{
		Match->Step(Dt);
		TArray<int32> Ids;
		for (const FCytraEnemy& Enemy : Match->GetEnemies())
		{
			Ids.Add(Enemy.Id);
		}
		for (const int32 Id : Ids)
		{
			Match->DamageEnemy(Id, 1000.0);
		}
	}
	End = FCytraEndView::Build(*Match);
	if (TestTrue(TEXT("Won"), End.IsSet()))
	{
		TestTrue(TEXT("Won flag"), End->bWon);
		TestEqual(TEXT("Won title"), End->Title.ToString(), FString(TEXT("La couvée est sauve")));
		TestEqual(TEXT("Won text"), End->Body.ToString(), FString(TEXT("Dix vagues repoussées, 12 œufs sur 12 intacts. Cytra s’en souviendra.")));
		TestEqual(TEXT("Won progress"), End->Progress.ToString(), FString(TEXT("Vague 1/1 · 12 œufs restants")));
		TestEqual(TEXT("Won stats"), End->Stats.ToString(), FString(TEXT("8 assaillants abattus · 0 or dépensé · 0 or extrait · 0 tourelle")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMatchEffectsTest, "Cytra.Rules.Match.GoldAndLeakEffects", CytraTest::Flags)

bool FCytraMatchEffectsTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	// A kill shows the gold it pays where the enemy fell.
	Match->LaunchWave();
	for (int32 i = 0; i < 60 * 10 && Match->GetEnemies().IsEmpty(); ++i)
	{
		Match->Step(Dt);
	}
	if (!TestFalse(TEXT("An enemy spawned"), Match->GetEnemies().IsEmpty()))
	{
		return false;
	}
	Match->ConsumeEffects();
	const FCytraEnemy Victim = Match->GetEnemies()[0];
	Match->DamageEnemy(Victim.Id, 1000.0);
	TArray<FCytraEffect> Effects = Match->ConsumeEffects();
	if (TestEqual(TEXT("One effect"), Effects.Num(), 1))
	{
		TestTrue(TEXT("Loot"), Effects[0].Kind == ECytraEffectKind::Loot);
		TestEqual(TEXT("Loot gold"), Effects[0].Gold, 6);
		TestEqual(TEXT("Loot where the enemy fell"), Effects[0].To, Victim.Position);
	}

	// An enemy reaching the nest rings it.
	const TOptional<FCytraMatchEvent> Leak = StepUntilEvent(*Match, [](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::Leak; });
	if (TestTrue(TEXT("A bandit reached the nest"), Leak.IsSet()))
	{
		const TArray<FCytraEffect> LeakEffects = Match->ConsumeEffects();
		const FCytraEffect* Ring = LeakEffects.FindByPredicate([](const FCytraEffect& Effect) { return Effect.Kind == ECytraEffectKind::Leak; });
		TestTrue(TEXT("Leak effect on the nest"), Ring && Ring->To == FVector2d(22.0, 7.0));
	}

	// Gold from the mine shows on the mine.
	Match = MakeMatch(*this);
	if (!Match || !TestTrue(TEXT("Open mine accepted"), Match->Order(FIntPoint(14, 11), FCytraOrderAction::Work(TEXT("OpenMine"))).IsAccepted()))
	{
		return false;
	}
	const TOptional<FCytraMatchEvent> Yield = StepUntilEvent(*Match, [](const FCytraMatchEvent& Event) { return Event.Type == ECytraMatchEventType::MineYield; });
	if (TestTrue(TEXT("Mine yielded"), Yield.IsSet()))
	{
		const TArray<FCytraEffect> MineEffects = Match->ConsumeEffects();
		const FCytraEffect* Gold = MineEffects.FindByPredicate([](const FCytraEffect& Effect) { return Effect.Kind == ECytraEffectKind::MineGold; });
		if (TestNotNull(TEXT("Mine gold effect"), Gold))
		{
			TestEqual(TEXT("Mine gold"), Gold->Gold, 2);
			TestEqual(TEXT("On the mine"), Gold->To, FVector2d(14.0, 11.0));
		}
	}
	return true;
}

} // namespace CytraInterfaceTest

#endif
