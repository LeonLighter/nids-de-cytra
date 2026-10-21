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
	TestTrue(TEXT("Leading # accepted"), FCytraLooks::ParseJson(TEXT(R"({"enemies":{"Bandit":{"shape":"Cone","color":"#A9814E","size":0.4}}})"), Looks, Error));
	return true;
}

#endif
