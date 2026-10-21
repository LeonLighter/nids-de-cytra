// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "UI/CytraText.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTextKeysPresentTest, "Cytra.UI.Text.KeysPresent", CytraTest::Flags)

bool FCytraTextKeysPresentTest::RunTest(const FString& Parameters)
{
	const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(CytraText::TableId);
	if (!TestTrue(TEXT("String table registered"), Table.IsValid()))
	{
		return false;
	}
	for (int32 i = 0; i < CytraText::NumKeys; ++i)
	{
		FString Source;
		TestTrue(FString::Printf(TEXT("Key %s present"), CytraText::Keys[i]), Table->GetSourceString(CytraText::Keys[i], Source) && !Source.IsEmpty());
	}

	// The file is UTF-8: accented letters must survive.
	TestEqual(TEXT("Eggs label"), CytraText::Get(TEXT("Hud.Eggs")).ToString(), FString(TEXT("Œufs")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTextFormatsTest, "Cytra.UI.Text.Formats", CytraTest::Flags)

bool FCytraTextFormatsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Wave value"), FText::Format(CytraText::Get(TEXT("Hud.WaveValue")), 3, 10).ToString(), FString(TEXT("3/10")));
	TestEqual(TEXT("Countdown"), FText::Format(CytraText::Get(TEXT("Hud.NextIn")), 2, 18).ToString(), FString(TEXT("Vague 2 dans 18 s.")));
	TestEqual(TEXT("Launch now"), FText::Format(CytraText::Get(TEXT("Hud.LaunchNow")), 18).ToString(), FString(TEXT("Lancer maintenant (+18 or)")));

	// Plural forms follow the current culture; the prototype uses the singular up to 1, as French does.
	const FString Seven = FText::Format(CytraText::Get(TEXT("Hud.WaveRunning")), 2, 7).ToString();
	TestEqual(TEXT("Several enemies left"), Seven, FString(TEXT("Vague 2 en cours. 7 assaillants encore debout.")));
	const FString One = FText::Format(CytraText::Get(TEXT("Hud.WaveRunning")), 2, 1).ToString();
	TestEqual(TEXT("One enemy left"), One, FString(TEXT("Vague 2 en cours. 1 assaillant encore debout.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraTextDataKeysTest, "Cytra.UI.Text.DataKeys", CytraTest::Flags)

bool FCytraTextDataKeysTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	// A specialty or a work added to the data needs its texts too.
	TArray<FString> Keys;
	for (const TPair<FName, FCytraRoleDef>& Role : Balance.Roles)
	{
		Keys.Add(FString::Printf(TEXT("Role.%s.Short"), *Role.Key.ToString()));
	}
	for (const TPair<FName, FCytraWorkDef>& Work : Balance.Works)
	{
		Keys.Add(FString::Printf(TEXT("Work.%s.Label"), *Work.Key.ToString()));
		Keys.Add(FString::Printf(TEXT("Work.%s.Ordered"), *Work.Key.ToString()));
	}
	const UEnum* TileEnum = StaticEnum<ECytraTileType>();
	for (int32 i = 0; i < TileEnum->NumEnums() - 1; ++i)
	{
		const ECytraTileType Type = static_cast<ECytraTileType>(TileEnum->GetValueByIndex(i));
		const FString Name = TileEnum->GetNameStringByIndex(i);
		Keys.Add(FString::Printf(TEXT("Tile.%s.Title"), *Name));
		if (!IsCytraSlot(Type))
		{
			Keys.Add(FString::Printf(TEXT("Tile.%s.Desc"), *Name));
		}
		if (IsCytraObstacle(Type) || Type == ECytraTileType::OutpostSite)
		{
			Keys.Add(FString::Printf(TEXT("Tile.%s.Opened"), *Name));
		}
	}
	for (const FString& Key : Keys)
	{
		TestTrue(FString::Printf(TEXT("Key %s present"), *Key), CytraText::Has(Key));
	}
	return true;
}

#endif
