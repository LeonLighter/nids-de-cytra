// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraRoutePath.h"

namespace
{
	void TestPosition(FAutomationTestBase& Test, const FString& What, const FVector2d& Actual, const FVector2d& Expected)
	{
		Test.TestTrue(FString::Printf(TEXT("%s: %s, expected %s"), *What, *Actual.ToString(), *Expected.ToString()), Actual.Equals(Expected, 1e-9));
	}
}

// GDD section 13, case 2.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraRouteLengthTest, "Cytra.Rules.Route.Length", CytraTest::Flags)

bool FCytraRouteLengthTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	for (const FCytraRoute& Route : Map.Routes)
	{
		const FCytraRoutePath Path(Route);
		TestEqual(FString::Printf(TEXT("%s length"), *Route.Id.ToString()), Path.GetLength(), 27.0);
		TestPosition(*this, FString::Printf(TEXT("%s spawn"), *Route.Id.ToString()), Path.PositionAt(0.0), FVector2d(Route.Waypoints[0]));
		TestPosition(*this, FString::Printf(TEXT("%s end at the nest"), *Route.Id.ToString()), Path.PositionAt(27.0), FVector2d(Map.Nest));
		TestPosition(*this, FString::Printf(TEXT("%s beyond the end"), *Route.Id.ToString()), Path.PositionAt(40.0), FVector2d(Map.Nest));
		TestPosition(*this, FString::Printf(TEXT("%s before the start"), *Route.Id.ToString()), Path.PositionAt(-1.0), FVector2d(Route.Waypoints[0]));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraRoutePositionTest, "Cytra.Rules.Route.PositionAt", CytraTest::Flags)

bool FCytraRoutePositionTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	FCytraBalance Balance;
	if (!CytraTest::LoadMapAndBalance(*this, Map, Balance))
	{
		return false;
	}

	// North: (-1,3) -> (6,3) -> (6,5) -> (17,5) -> (17,7) -> (22,7).
	const FCytraRoutePath North(Map.Routes[0]);
	TestPosition(*this, TEXT("Entering the map"), North.PositionAt(0.6), FVector2d(-0.4, 3.0));
	TestPosition(*this, TEXT("First corner"), North.PositionAt(7.0), FVector2d(6.0, 3.0));
	TestPosition(*this, TEXT("Going south"), North.PositionAt(8.5), FVector2d(6.0, 4.5));
	TestPosition(*this, TEXT("Junction"), North.PositionAt(22.0), FVector2d(17.0, 7.0));
	TestPosition(*this, TEXT("Last stretch"), North.PositionAt(24.25), FVector2d(19.25, 7.0));

	// South: (-1,11) -> (4,11) -> (4,9) -> (17,9) -> (17,7) -> (22,7).
	const FCytraRoutePath South(Map.Routes[1]);
	TestPosition(*this, TEXT("South corner"), South.PositionAt(5.0), FVector2d(4.0, 11.0));
	TestPosition(*this, TEXT("South going north"), South.PositionAt(6.0), FVector2d(4.0, 10.0));
	TestPosition(*this, TEXT("South junction"), South.PositionAt(22.0), FVector2d(17.0, 7.0));
	return true;
}

#endif
