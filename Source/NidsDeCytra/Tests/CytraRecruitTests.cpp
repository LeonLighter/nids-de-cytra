// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatch.h"
#include "Rules/CytraWaveSchedule.h"

namespace CytraRecruitTest
{
	const FName Carpenter(TEXT("Carpenter"));
	const FName Miner(TEXT("Miner"));
	const FName Engineer(TEXT("Engineer"));

	/** A match on the real data, optionally with another starting crew and gold. */
	TOptional<FCytraMatch> MakeMatch(FAutomationTestBase& Test, const TArray<FName>* StartRoles = nullptr, TOptional<int32> StartGold = {})
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
		if (StartGold)
		{
			Balance.StartGold = *StartGold;
		}
		return FCytraMatch(Map, Balance, Waves);
	}

// The tests live in the namespace too: unity builds merge test files, and other files have helpers with the same names.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewRecruitTest, "Cytra.Rules.Crew.Recruit", CytraTest::Flags)

bool FCytraCrewRecruitTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}

	// The coil needs an engineer, who is not in the starting crew.
	const FIntPoint Slot(3, 2);
	TestTrue(TEXT("Coil refused without an engineer"), Match->CheckOrder(Slot, FCytraOrderAction::Build(TEXT("Coil"))).Refusal == ECytraOrderRefusal::MissingSpecialty);

	const FCytraRecruitCheck Check = Match->CheckRecruit(Engineer);
	TestTrue(TEXT("Engineer recruitable"), Check.IsAccepted());
	TestEqual(TEXT("Engineer cost"), Check.Cost, 60);

	const FCytraRecruitCheck Recruited = Match->Recruit(Engineer);
	if (!TestTrue(TEXT("Engineer recruited"), Recruited.IsAccepted()))
	{
		return false;
	}
	TestEqual(TEXT("Gold paid"), Match->GetWallet().Gold, 110);
	TestEqual(TEXT("Counted as spent"), Match->GetWallet().Spent, 60);
	TestEqual(TEXT("Crew of three"), Match->GetWorkers().Num(), 3);
	const FCytraWorker* Recruit = Match->FindWorker(Recruited.Worker);
	if (TestNotNull(TEXT("Recruit found"), Recruit))
	{
		TestEqual(TEXT("Recruit specialty"), Recruit->Role, Engineer);
		TestTrue(TEXT("Arrives at the main camp"), Recruit->Position == FVector2d(Match->GetMap().Camp.X, Match->GetMap().Camp.Y));
		TestTrue(TEXT("Waits there"), Recruit->State == ECytraWorkerState::Idle);
	}
	TestTrue(TEXT("Coil accepted with an engineer"), Match->CheckOrder(Slot, FCytraOrderAction::Build(TEXT("Coil"))).IsAccepted());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewRecruitRefusalsTest, "Cytra.Rules.Crew.RecruitRefusals", CytraTest::Flags)

bool FCytraCrewRecruitRefusalsTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	TestTrue(TEXT("Unknown specialty"), Match->CheckRecruit(TEXT("Blacksmith")).Refusal == ECytraRecruitRefusal::UnknownRole);

	// 170 gold: an engineer (60) and a miner (40) leave 70, then a carpenter (50) leaves 20, and a second one is short of 30.
	Match->Recruit(Engineer);
	Match->Recruit(Miner);
	Match->Recruit(Carpenter);
	const FCytraRecruitCheck Short = Match->Recruit(Carpenter);
	TestTrue(TEXT("Not enough gold"), Short.Refusal == ECytraRecruitRefusal::NotEnoughGold);
	TestEqual(TEXT("Missing gold"), Short.MissingGold, 30);
	TestEqual(TEXT("Nothing paid"), Match->GetWallet().Gold, 20);
	TestEqual(TEXT("Nobody added"), Match->GetWorkers().Num(), 5);

	// A full crew of 8 is refused first, even without gold.
	const TArray<FName> Full = { Carpenter, Miner, Engineer, Carpenter, Miner, Engineer, Carpenter, Miner };
	Match = MakeMatch(*this, &Full, 0);
	if (!Match)
	{
		return false;
	}
	TestTrue(TEXT("Crew full before gold"), Match->CheckRecruit(Miner).Refusal == ECytraRecruitRefusal::CrewFull);
	TestFalse(TEXT("Full crew refused"), Match->Recruit(Miner).IsAccepted());
	TestEqual(TEXT("Still 8"), Match->GetWorkers().Num(), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraCrewRecruitTakesJobTest, "Cytra.Rules.Crew.RecruitTakesJob", CytraTest::Flags)

bool FCytraCrewRecruitTakesJobTest::RunTest(const FString& Parameters)
{
	TOptional<FCytraMatch> Match = MakeMatch(*this);
	if (!Match)
	{
		return false;
	}
	// The only carpenter takes the bridge; the ballista waits for a carpenter.
	Match->Order(FIntPoint(9, 6), FCytraOrderAction::Work(TEXT("Bridge")));
	const FCytraOrderResult Ballista = Match->Order(FIntPoint(19, 6), FCytraOrderAction::Build(TEXT("Ballista")));
	if (!TestTrue(TEXT("Ballista waits"), Ballista.IsAccepted() && Ballista.bWaiting))
	{
		return false;
	}

	// A carpenter recruited now takes it at once.
	const FCytraRecruitCheck Recruited = Match->Recruit(Carpenter);
	TestTrue(TEXT("Carpenter recruited"), Recruited.IsAccepted());
	TestEqual(TEXT("The recruit takes the waiting job"), Match->FindJobAt(FIntPoint(19, 6))->Worker, Recruited.Worker);
	TestTrue(TEXT("On the way"), Match->FindWorker(Recruited.Worker)->State == ECytraWorkerState::Move);
	return true;
}

}

#endif
