// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "World/CytraLooks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraLooksLoadsLooksTest, "Cytra.World.Looks.LoadsLooks", CytraTest::Flags)

bool FCytraLooksLoadsLooksTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	FCytraLooks Looks;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("Looks loaded (%s)"), *Error), FCytraLooks::LoadFromFile(CytraTest::DataPath(TEXT("Looks.json")), Looks, Error)))
	{
		return false;
	}
	for (const TPair<FName, FCytraEnemyDef>& Enemy : Balance.Enemies)
	{
		TestTrue(FString::Printf(TEXT("%s has a look"), *Enemy.Key.ToString()), Looks.Enemies.Contains(Enemy.Key));
	}
	for (const TPair<FName, FCytraRoleDef>& Role : Balance.Roles)
	{
		TestTrue(FString::Printf(TEXT("%s has a look"), *Role.Key.ToString()), Looks.Workers.Contains(Role.Key));
	}
	for (const TPair<FName, FCytraTowerDef>& Tower : Balance.Towers)
	{
		TestTrue(FString::Printf(TEXT("%s has a look"), *Tower.Key.ToString()), Looks.Towers.Contains(Tower.Key));
	}
	TestTrue(TEXT("Ballista aims"), Looks.TowerLook(TEXT("Ballista")).bAims);
	TestFalse(TEXT("Mortar does not aim"), Looks.TowerLook(TEXT("Mortar")).bAims);
	TestEqual(TEXT("Coil top"), Looks.TowerLook(TEXT("Coil")).Top.Shape, FName(TEXT("Sphere")));
	TestTrue(TEXT("Carpenter color"), Looks.WorkerLook(TEXT("Carpenter")).Color.Equals(FLinearColor::FromSRGBColor(FColor(0xd1, 0x7f, 0x45))));

	const FCytraLook& Bandit = Looks.EnemyLook(TEXT("Bandit"));
	TestEqual(TEXT("Bandit shape"), Bandit.Shape, FName(TEXT("Sphere")));
	TestEqual(TEXT("Bandit size"), Bandit.Size, 0.35);
	TestTrue(TEXT("Bandit color"), Bandit.Color.Equals(FLinearColor::FromSRGBColor(FColor(0xa9, 0x81, 0x4e))));

	const FCytraLook& Unknown = Looks.EnemyLook(TEXT("Dragon"));
	TestEqual(TEXT("Default shape for an unknown enemy"), Unknown.Shape, FName(TEXT("Sphere")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraLooksRejectsBadLooksTest, "Cytra.World.Looks.RejectsBadLooks", CytraTest::Flags)

bool FCytraLooksRejectsBadLooksTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* What;
		const TCHAR* Json;
		const TCHAR* ExpectedField;
	};
	const FCase Cases[] = {
		{ TEXT("Unknown shape"), TEXT(R"({"enemies":{"Bandit":{"shape":"Torus","color":"a9814e","size":0.35}}})"), TEXT("enemies.Bandit.shape") },
		{ TEXT("Bad color"), TEXT(R"({"enemies":{"Bandit":{"shape":"Cube","color":"zz814e","size":0.35}}})"), TEXT("enemies.Bandit.color") },
		{ TEXT("Short color"), TEXT(R"({"enemies":{"Bandit":{"shape":"Cube","color":"fff","size":0.35}}})"), TEXT("enemies.Bandit.color") },
		{ TEXT("Zero size"), TEXT(R"({"enemies":{"Bandit":{"shape":"Cube","color":"a9814e","size":0}}})"), TEXT("enemies.Bandit.size") },
		{ TEXT("Missing size"), TEXT(R"({"enemies":{"Bandit":{"shape":"Cube","color":"a9814e"}}})"), TEXT("enemies.Bandit.size") },
		{ TEXT("No enemies section"), TEXT(R"({})"), TEXT("enemies") },
		{ TEXT("No workers section"), TEXT(R"({"enemies":{}})"), TEXT("workers") },
		{ TEXT("Bad worker"), TEXT(R"({"enemies":{},"workers":{"Miner":{"shape":"Cube","color":"e3b341","size":-1}}})"), TEXT("workers.Miner.size") },
		{ TEXT("No towers section"), TEXT(R"({"enemies":{},"workers":{}})"), TEXT("towers") },
		{ TEXT("Tower without top"), TEXT(R"({"enemies":{},"workers":{},"towers":{"Mortar":{"base":{"shape":"Cube","color":"3a404a","size":0.6}}}})"), TEXT("towers.Mortar.top") },
		{ TEXT("Bad tower base"), TEXT(R"({"enemies":{},"workers":{},"towers":{"Mortar":{"base":{"shape":"Torus","color":"3a404a","size":0.6},"top":{"shape":"Cube","color":"07090c","size":0.3}}}})"), TEXT("towers.Mortar.base.shape") },
		{ TEXT("Aims not a boolean"), TEXT(R"({"enemies":{},"workers":{},"towers":{"Ballista":{"base":{"shape":"Cube","color":"3a404a","size":0.6},"top":{"shape":"Cube","color":"07090c","size":0.3},"aims":1}}})"), TEXT("towers.Ballista.aims") },
	};
	for (const FCase& Case : Cases)
	{
		FCytraLooks Looks;
		FString Error;
		TestFalse(FString::Printf(TEXT("%s rejected"), Case.What), FCytraLooks::ParseJson(Case.Json, Looks, Error));
		TestTrue(FString::Printf(TEXT("%s: error names %s (%s)"), Case.What, Case.ExpectedField, *Error), Error.StartsWith(Case.ExpectedField));
	}

	FCytraLooks Looks;
	FString Error;
	TestTrue(TEXT("Leading # accepted"), FCytraLooks::ParseJson(TEXT(R"({"enemies":{"Bandit":{"shape":"Cone","color":"#A9814E","size":0.4}},"workers":{},"towers":{}})"), Looks, Error));
	return true;
}

#endif
