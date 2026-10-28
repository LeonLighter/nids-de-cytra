// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraBalanceLoader.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraWaveLoader.h"
#include "Rules/CytraWaveSchedule.h"

namespace
{
	/** Breaks the real balance file with Mutate and expects a rejection mentioning ExpectedFragment. */
	void ExpectBalanceRejected(FAutomationTestBase& Test, const TCHAR* What, TFunctionRef<void(FJsonObject&)> Mutate, const TCHAR* ExpectedFragment)
	{
		const TSharedPtr<FJsonObject> Root = CytraTest::ReadJsonObject(TEXT("Balance.json"));
		if (!Test.TestValid(TEXT("Balance.json readable"), Root))
		{
			return;
		}
		Mutate(*Root);

		FCytraBalance Balance;
		FString Error;
		if (Test.TestFalse(What, FCytraBalanceLoader::ParseJson(CytraTest::WriteJson(Root), Balance, Error)))
		{
			Test.TestTrue(FString::Printf(TEXT("%s: error '%s' mentions '%s'"), What, *Error, ExpectedFragment), Error.Contains(ExpectedFragment));
		}
	}

	/** Same for the wave file; Mutate receives the first group of the first wave and the root. */
	void ExpectWavesRejected(FAutomationTestBase& Test, const TCHAR* What, TFunctionRef<void(FJsonObject& Root, FJsonObject& FirstGroup)> Mutate, const TCHAR* ExpectedFragment)
	{
		FCytraMapDef Map;
		FCytraBalance Balance;
		const TSharedPtr<FJsonObject> Root = CytraTest::ReadJsonObject(TEXT("Waves/Passe01.json"));
		if (!CytraTest::LoadMapAndBalance(Test, Map, Balance) || !Test.TestValid(TEXT("Waves readable"), Root))
		{
			return;
		}
		Mutate(*Root, *Root->GetArrayField(TEXT("waves"))[0]->AsArray()[0]->AsObject());

		FCytraWaveSchedule Waves;
		FString Error;
		if (Test.TestFalse(What, FCytraWaveLoader::ParseJson(CytraTest::WriteJson(Root), Balance, Map, Waves, Error)))
		{
			Test.TestTrue(FString::Printf(TEXT("%s: error '%s' mentions '%s'"), What, *Error, ExpectedFragment), Error.Contains(ExpectedFragment));
		}
	}

	TSharedPtr<FJsonObject> Entry(FJsonObject& Root, const TCHAR* Section, const TCHAR* Id)
	{
		return Root.GetObjectField(Section)->GetObjectField(Id);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraDataLoadsBalanceTest, "Cytra.Rules.Data.LoadsBalance", CytraTest::Flags)

bool FCytraDataLoadsBalanceTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	TestEqual(TEXT("Start gold"), Balance.StartGold, 170);
	TestEqual(TEXT("Start eggs"), Balance.StartEggs, 12);
	TestEqual(TEXT("Max crew"), Balance.MaxCrew, 8);
	TestEqual(TEXT("Dismissal refund"), Balance.DismissRefund, 0.7);
	TestEqual(TEXT("Start crew, carpenter first"), Balance.StartRoles, TArray<FName>{ TEXT("Carpenter"), TEXT("Miner") });
	TestEqual(TEXT("Roles"), Balance.Roles.Num(), 3);
	TestEqual(TEXT("Engineer recruit cost"), Balance.Roles.FindChecked(TEXT("Engineer")).RecruitCost, 60);
	TestEqual(TEXT("Works"), Balance.Works.Num(), 5);
	TestEqual(TEXT("Bridge opens gaps"), Balance.Works.FindChecked(TEXT("Bridge")).On, ECytraTileType::Gap);
	TestEqual(TEXT("Outpost time"), Balance.Works.FindChecked(TEXT("Outpost")).Time, 10.0);
	TestEqual(TEXT("Towers"), Balance.Towers.Num(), 3);
	TestEqual(TEXT("Mortar behavior"), Balance.Towers.FindChecked(TEXT("Mortar")).Behavior, ECytraTowerBehavior::Shell);
	TestEqual(TEXT("Coil targets"), Balance.Towers.FindChecked(TEXT("Coil")).Targets, 3);
	TestEqual(TEXT("Coil role"), Balance.Towers.FindChecked(TEXT("Coil")).Role, FName(TEXT("Engineer")));
	TestEqual(TEXT("Enemies"), Balance.Enemies.Num(), 4);
	TestEqual(TEXT("Captain eggs"), Balance.Enemies.FindChecked(TEXT("Captain")).Eggs, 5);
	TestEqual(TEXT("Upgrade role"), Balance.Upgrade.Role, FName(TEXT("Engineer")));
	TestEqual(TEXT("Max level"), Balance.Upgrade.MaxLevel, 3);
	TestEqual(TEXT("Mine slots"), Balance.MineSlots, 2);
	TestEqual(TEXT("Targeting min x"), Balance.TargetingMinX, -0.4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraDataLoadsWavesTest, "Cytra.Rules.Data.LoadsWaves", CytraTest::Flags)

bool FCytraDataLoadsWavesTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	if (!CytraTest::LoadAll(*this, Map, Balance, Waves))
	{
		return false;
	}

	TestEqual(TEXT("Wave count"), Waves.Num(), 10);
	const int32 ExpectedGroups[] = { 1, 1, 2, 2, 2, 3, 3, 4, 4, 4 };
	for (int32 Index = 0; Index < Waves.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Groups in wave %d"), Index + 1), Waves.Waves[Index].Num(), ExpectedGroups[Index]);
	}

	// Wave 6, third group: one soldier after 8 s; the missing interval falls back to 1 s like in the prototype.
	const FCytraWaveGroup& LoneSoldier = Waves.Waves[5][2];
	TestEqual(TEXT("Lone soldier route"), LoneSoldier.Route, FName(TEXT("North")));
	TestEqual(TEXT("Lone soldier enemy"), LoneSoldier.Enemy, FName(TEXT("Soldier")));
	TestEqual(TEXT("Lone soldier delay"), LoneSoldier.Delay, 8.0);
	TestEqual(TEXT("Default interval"), LoneSoldier.Interval, 1.0);
	TestEqual(TEXT("Default delay"), Waves.Waves[0][0].Delay, 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraDataRejectsBadBalanceTest, "Cytra.Rules.Data.RejectsBadBalance", CytraTest::Flags)

bool FCytraDataRejectsBadBalanceTest::RunTest(const FString& Parameters)
{
	ExpectBalanceRejected(*this, TEXT("Unknown role"),
		[](FJsonObject& Root) { Entry(Root, TEXT("works"), TEXT("Clear"))->SetStringField(TEXT("role"), TEXT("Smith")); },
		TEXT("works.Clear.role: unknown role 'Smith'"));
	ExpectBalanceRejected(*this, TEXT("Unknown start role"),
		[](FJsonObject& Root) { Root.GetObjectField(TEXT("crew"))->SetArrayField(TEXT("startRoles"), { MakeShared<FJsonValueString>(TEXT("Smith")) }); },
		TEXT("crew.startRoles: unknown role 'Smith'"));
	ExpectBalanceRejected(*this, TEXT("Refund above the cost"),
		[](FJsonObject& Root) { Root.GetObjectField(TEXT("crew"))->SetNumberField(TEXT("dismissRefund"), 1.5); },
		TEXT("crew.dismissRefund: must be at most 1"));
	ExpectBalanceRejected(*this, TEXT("Work on a road"),
		[](FJsonObject& Root) { Entry(Root, TEXT("works"), TEXT("Bridge"))->SetStringField(TEXT("on"), TEXT("Road")); },
		TEXT("works.Bridge.on: 'Road' is not a tile type a job can open"));
	ExpectBalanceRejected(*this, TEXT("Unknown behavior"),
		[](FJsonObject& Root) { Entry(Root, TEXT("towers"), TEXT("Mortar"))->SetStringField(TEXT("behavior"), TEXT("Laser")); },
		TEXT("unknown behavior 'Laser'"));
	ExpectBalanceRejected(*this, TEXT("Shell without splash"),
		[](FJsonObject& Root) { Entry(Root, TEXT("towers"), TEXT("Mortar"))->RemoveField(TEXT("splash")); },
		TEXT("towers.Mortar.splash: missing number"));
	ExpectBalanceRejected(*this, TEXT("Fractional hit points"),
		[](FJsonObject& Root) { Entry(Root, TEXT("enemies"), TEXT("Bandit"))->SetNumberField(TEXT("hp"), 30.5); },
		TEXT("enemies.Bandit.hp: must be an integer"));
	ExpectBalanceRejected(*this, TEXT("Zero speed"),
		[](FJsonObject& Root) { Entry(Root, TEXT("enemies"), TEXT("Soldier"))->SetNumberField(TEXT("speed"), 0.0); },
		TEXT("enemies.Soldier.speed: must be positive"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraDataRejectsBadWavesTest, "Cytra.Rules.Data.RejectsBadWaves", CytraTest::Flags)

bool FCytraDataRejectsBadWavesTest::RunTest(const FString& Parameters)
{
	ExpectWavesRejected(*this, TEXT("Unknown enemy"),
		[](FJsonObject&, FJsonObject& Group) { Group.SetStringField(TEXT("enemy"), TEXT("Dragon")); },
		TEXT("waves[0][0].enemy: unknown enemy 'Dragon'"));
	ExpectWavesRejected(*this, TEXT("Unknown route"),
		[](FJsonObject&, FJsonObject& Group) { Group.SetStringField(TEXT("route"), TEXT("East")); },
		TEXT("waves[0][0].route: unknown route 'East'"));
	ExpectWavesRejected(*this, TEXT("Empty group"),
		[](FJsonObject&, FJsonObject& Group) { Group.SetNumberField(TEXT("count"), 0); },
		TEXT("waves[0][0].count: must be at least 1"));
	ExpectWavesRejected(*this, TEXT("Other map"),
		[](FJsonObject& Root, FJsonObject&) { Root.SetStringField(TEXT("map"), TEXT("Passe02")); },
		TEXT("waves are for map Passe02"));
	return true;
}

#endif
