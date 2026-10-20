// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMapLoader.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags CytraTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A tiny map: camp at (1,0), road along row 1 to the nest at (2,1). */
	const TArray<FString> MiniGrid = { TEXT("#C##"), TEXT("..N#"), TEXT("####") };
	const FString MiniRoute = TEXT("[[-1,1],[2,1]]");

	FString MakeMapJson(const TArray<FString>& Grid, const FString& Waypoints)
	{
		TArray<FString> Rows;
		for (const FString& Row : Grid)
		{
			Rows.Add(FString::Printf(TEXT("\"%s\""), *Row));
		}
		return FString::Printf(
			TEXT("{\"id\":\"Mini\",\"cols\":%d,\"rows\":%d,")
			TEXT("\"legend\":{\"#\":\"Rock\",\".\":\"Road\",\"C\":\"Camp\",\"N\":\"Nest\"},")
			TEXT("\"grid\":[%s],\"routes\":[{\"id\":\"West\",\"waypoints\":%s}]}"),
			Grid.Num() ? Grid[0].Len() : 0, Grid.Num(), *FString::Join(Rows, TEXT(",")), *Waypoints);
	}

	bool Parses(const FString& Json, FString& OutError)
	{
		FCytraMapDef Map;
		return FCytraMapLoader::ParseJson(Json, Map, OutError);
	}

	/** The map must be rejected, with an error that mentions ExpectedFragment. */
	void ExpectRejected(FAutomationTestBase& Test, const TCHAR* What, const FString& Json, const TCHAR* ExpectedFragment)
	{
		FString Error;
		if (Test.TestFalse(What, Parses(Json, Error)))
		{
			Test.TestTrue(FString::Printf(TEXT("%s: error '%s' mentions '%s'"), What, *Error, ExpectedFragment), Error.Contains(ExpectedFragment));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMapLoadsPasse01Test, "Cytra.Rules.Map.LoadsPasse01", CytraTestFlags)

bool FCytraMapLoadsPasse01Test::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FString Error;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Maps/Passe01.json");
	if (!FCytraMapLoader::LoadFromFile(Path, Map, Error))
	{
		AddError(Error);
		return false;
	}

	TestEqual(TEXT("Cols"), Map.Cols, 24);
	TestEqual(TEXT("Rows"), Map.Rows, 14);
	TestEqual(TEXT("Camp"), Map.Camp, FIntPoint(20, 9));
	TestEqual(TEXT("Nest"), Map.Nest, FIntPoint(22, 7));
	TestEqual(TEXT("Mines"), Map.Mines, TArray<FIntPoint>{ FIntPoint(14, 11) });
	TestEqual(TEXT("Outpost sites"), Map.OutpostSites, TArray<FIntPoint>{ FIntPoint(4, 7) });

	TestEqual(TEXT("Slots"), Map.Count(ECytraTileType::Slot), 11);
	TestEqual(TEXT("High slots"), Map.Count(ECytraTileType::HighSlot), 2);
	TestEqual(TEXT("Ledges"), Map.Count(ECytraTileType::Ledge), 8);
	TestEqual(TEXT("Rubble"), Map.Count(ECytraTileType::Rubble), 2);
	TestEqual(TEXT("Gaps"), Map.Count(ECytraTileType::Gap), 2);
	TestEqual(TEXT("Cliffs"), Map.Count(ECytraTileType::Cliff), 2);

	TestEqual(TEXT("High slot (15,7)"), Map.Get(FIntPoint(15, 7)), ECytraTileType::HighSlot);
	TestEqual(TEXT("High slot (8,3)"), Map.Get(FIntPoint(8, 3)), ECytraTileType::HighSlot);
	TestEqual(TEXT("Cliff (14,7)"), Map.Get(FIntPoint(14, 7)), ECytraTileType::Cliff);
	TestEqual(TEXT("Rubble (13,8)"), Map.Get(FIntPoint(13, 8)), ECytraTileType::Rubble);
	TestEqual(TEXT("Gap (9,6)"), Map.Get(FIntPoint(9, 6)), ECytraTileType::Gap);
	TestEqual(TEXT("Off-grid reads as rock"), Map.Get(FIntPoint(-1, 3)), ECytraTileType::Rock);

	if (TestEqual(TEXT("Routes"), Map.Routes.Num(), 2))
	{
		TestEqual(TEXT("North id"), Map.Routes[0].Id, FName(TEXT("North")));
		TestEqual(TEXT("North start"), Map.Routes[0].Waypoints[0], FIntPoint(-1, 3));
		TestEqual(TEXT("South id"), Map.Routes[1].Id, FName(TEXT("South")));
		TestEqual(TEXT("South start"), Map.Routes[1].Waypoints[0], FIntPoint(-1, 11));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMapAcceptsMiniTest, "Cytra.Rules.Map.AcceptsMinimalMap", CytraTestFlags)

bool FCytraMapAcceptsMiniTest::RunTest(const FString& Parameters)
{
	FString Error;
	if (!Parses(MakeMapJson(MiniGrid, MiniRoute), Error))
	{
		AddError(FString::Printf(TEXT("Minimal map rejected: %s"), *Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMapRejectsBadGridTest, "Cytra.Rules.Map.RejectsBadGrid", CytraTestFlags)

bool FCytraMapRejectsBadGridTest::RunTest(const FString& Parameters)
{
	ExpectRejected(*this, TEXT("Not JSON"), TEXT("{ nope"), TEXT("invalid JSON"));
	ExpectRejected(*this, TEXT("Short row"), MakeMapJson({ TEXT("#C##"), TEXT("..N"), TEXT("####") }, MiniRoute), TEXT("row 1"));
	ExpectRejected(*this, TEXT("Unknown symbol"), MakeMapJson({ TEXT("#C#X"), TEXT("..N#"), TEXT("####") }, MiniRoute), TEXT("unknown symbol 'X'"));
	ExpectRejected(*this, TEXT("Two camps"), MakeMapJson({ TEXT("#CC#"), TEXT("..N#"), TEXT("####") }, MiniRoute), TEXT("second Camp"));
	ExpectRejected(*this, TEXT("No nest"), MakeMapJson({ TEXT("#C##"), TEXT("...#"), TEXT("####") }, MiniRoute), TEXT("one camp and one nest"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraMapRejectsBadRouteTest, "Cytra.Rules.Map.RejectsBadRoute", CytraTestFlags)

bool FCytraMapRejectsBadRouteTest::RunTest(const FString& Parameters)
{
	ExpectRejected(*this, TEXT("Diagonal segment"), MakeMapJson(MiniGrid, TEXT("[[-1,2],[2,1]]")), TEXT("not horizontal or vertical"));
	ExpectRejected(*this, TEXT("Does not end on the nest"), MakeMapJson(MiniGrid, TEXT("[[-1,1],[1,1]]")), TEXT("does not end on the nest"));
	ExpectRejected(*this, TEXT("Crosses rock"), MakeMapJson({ TEXT("#C##"), TEXT(".#.N"), TEXT("####") }, TEXT("[[-1,1],[3,1]]")), TEXT("crosses Rock at (1,1)"));
	ExpectRejected(*this, TEXT("Bad waypoint"), MakeMapJson(MiniGrid, TEXT("[[-1],[2,1]]")), TEXT("[x, y] pairs"));
	return true;
}

#endif
