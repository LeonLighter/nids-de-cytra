// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Rules/CytraBalance.h"
#include "Rules/CytraGrid.h"
#include "Rules/CytraMapDef.h"

namespace
{
	/** Every slot of the map that is currently reachable, in row-major order. */
	TArray<FIntPoint> ReachableSlots(const FCytraGrid& Grid)
	{
		TArray<FIntPoint> Slots;
		const FCytraMapDef& Map = Grid.GetMap();
		for (int32 Y = 0; Y < Map.Rows; ++Y)
		{
			for (int32 X = 0; X < Map.Cols; ++X)
			{
				if (IsCytraSlot(Map.Get(FIntPoint(X, Y))) && Grid.IsReachable(FIntPoint(X, Y)))
				{
					Slots.Add(FIntPoint(X, Y));
				}
			}
		}
		return Slots;
	}

	bool LoadMap(FAutomationTestBase& Test, FCytraMapDef& OutMap)
	{
		FCytraBalance Balance;
		return CytraTest::LoadMapAndBalance(Test, OutMap, Balance);
	}
}

// GDD section 13, case 1.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraGridReachAtStartTest, "Cytra.Rules.Grid.ReachAtStart", CytraTest::Flags)

bool FCytraGridReachAtStartTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	if (!LoadMap(*this, Map))
	{
		return false;
	}
	const FCytraGrid Grid(Map);

	TestEqual(TEXT("Reachable slots"), Grid.ReachableSlotCount(), 5);
	TestEqual(TEXT("Which slots"), ReachableSlots(Grid),
		TArray<FIntPoint>{ FIntPoint(3, 2), FIntPoint(11, 4), FIntPoint(19, 6), FIntPoint(10, 10), FIntPoint(2, 12) });
	TestTrue(TEXT("Camp reachable"), Grid.IsReachable(Map.Camp));
	TestTrue(TEXT("Nest reachable"), Grid.IsReachable(Map.Nest));
	TestFalse(TEXT("Outpost site not walkable yet"), Grid.IsWalkable(FIntPoint(4, 7)));
	TestTrue(TEXT("Outpost site has a reachable neighbour"), Grid.IsAdjacentToReach(FIntPoint(4, 7)));
	TestTrue(TEXT("Mine has a reachable neighbour"), Grid.IsAdjacentToReach(FIntPoint(14, 11)));
	TestFalse(TEXT("Mine never walkable"), Grid.IsWalkable(FIntPoint(14, 11)));
	return true;
}

// GDD section 13, case 3.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraGridAccessChainTest, "Cytra.Rules.Grid.AccessChain", CytraTest::Flags)

bool FCytraGridAccessChainTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	if (!LoadMap(*this, Map))
	{
		return false;
	}
	FCytraGrid Grid(Map);

	TestEqual(TEXT("(15,7): rubble then cliff"), Grid.AccessChain(FIntPoint(15, 7)), TArray<FIntPoint>{ FIntPoint(13, 8), FIntPoint(14, 7) });
	TestEqual(TEXT("(8,7): gap"), Grid.AccessChain(FIntPoint(8, 7)), TArray<FIntPoint>{ FIntPoint(9, 6) });
	TestEqual(TEXT("(10,7): gap"), Grid.AccessChain(FIntPoint(10, 7)), TArray<FIntPoint>{ FIntPoint(9, 6) });
	TestEqual(TEXT("(13,7): rubble"), Grid.AccessChain(FIntPoint(13, 7)), TArray<FIntPoint>{ FIntPoint(13, 8) });
	TestEqual(TEXT("(8,3): cliff"), Grid.AccessChain(FIntPoint(8, 3)), TArray<FIntPoint>{ FIntPoint(7, 3) });
	TestEqual(TEXT("(2,9): rubble"), Grid.AccessChain(FIntPoint(2, 9)), TArray<FIntPoint>{ FIntPoint(2, 10) });
	TestEqual(TEXT("(6,11): gap"), Grid.AccessChain(FIntPoint(6, 11)), TArray<FIntPoint>{ FIntPoint(7, 10) });
	TestTrue(TEXT("Reachable slot: empty chain"), Grid.AccessChain(FIntPoint(19, 6)).IsEmpty());

	Grid.Open(FIntPoint(13, 8));
	TestEqual(TEXT("(15,7) after clearing: cliff only"), Grid.AccessChain(FIntPoint(15, 7)), TArray<FIntPoint>{ FIntPoint(14, 7) });
	return true;
}

// GDD section 13, case 4.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraGridReachAfterOpeningTest, "Cytra.Rules.Grid.ReachAfterOpening", CytraTest::Flags)

bool FCytraGridReachAfterOpeningTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	if (!LoadMap(*this, Map))
	{
		return false;
	}

	FCytraGrid Bridged(Map);
	Bridged.Open(FIntPoint(9, 6));
	TestEqual(TEXT("After the bridge at (9,6)"), Bridged.ReachableSlotCount(), 7);
	TestTrue(TEXT("(8,7) reachable"), Bridged.IsReachable(FIntPoint(8, 7)));
	TestTrue(TEXT("(10,7) reachable"), Bridged.IsReachable(FIntPoint(10, 7)));

	FCytraGrid Cleared(Map);
	Cleared.Open(FIntPoint(13, 8));
	TestEqual(TEXT("After clearing (13,8) alone"), Cleared.ReachableSlotCount(), 6);
	TestTrue(TEXT("(13,7) reachable"), Cleared.IsReachable(FIntPoint(13, 7)));
	TestFalse(TEXT("(15,7) still out of reach"), Cleared.IsReachable(FIntPoint(15, 7)));

	Cleared.Open(FIntPoint(14, 7));
	TestEqual(TEXT("Then the ladder at (14,7)"), Cleared.ReachableSlotCount(), 7);
	TestTrue(TEXT("(15,7) reachable"), Cleared.IsReachable(FIntPoint(15, 7)));

	FCytraGrid Outpost(Map);
	TestFalse(TEXT("Outpost site is not a camp before"), Outpost.IsCamp(FIntPoint(4, 7)));
	Outpost.Open(FIntPoint(4, 7));
	TestTrue(TEXT("Raised outpost is walkable"), Outpost.IsWalkable(FIntPoint(4, 7)));
	TestTrue(TEXT("Raised outpost is a camp"), Outpost.IsCamp(FIntPoint(4, 7)));
	TestEqual(TEXT("Outpost opens no slot"), Outpost.ReachableSlotCount(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraGridFindPathTest, "Cytra.Rules.Grid.FindPath", CytraTest::Flags)

bool FCytraGridFindPathTest::RunTest(const FString& Parameters)
{
	FCytraMapDef Map;
	if (!LoadMap(*this, Map))
	{
		return false;
	}
	const FCytraGrid Grid(Map);

	const TOptional<TArray<FIntPoint>> ToSlot = Grid.FindPath(Map.Camp, [](const FIntPoint& Cell) { return Cell == FIntPoint(19, 6); });
	if (TestTrue(TEXT("Camp to (19,6) found"), ToSlot.IsSet()))
	{
		TestEqual(TEXT("Camp to (19,6)"), *ToSlot, TArray<FIntPoint>{ FIntPoint(20, 8), FIntPoint(20, 7), FIntPoint(19, 7), FIntPoint(19, 6) });
	}

	const TOptional<TArray<FIntPoint>> AlreadyThere = Grid.FindPath(Map.Camp, [&Map](const FIntPoint& Cell) { return Cell == Map.Camp; });
	TestTrue(TEXT("Start satisfies goal: empty path"), AlreadyThere.IsSet() && AlreadyThere->IsEmpty());

	// Three goals two steps from the junction: east is explored first, as in the prototype.
	const TOptional<TArray<FIntPoint>> Tie = Grid.FindPath(FIntPoint(17, 7), [](const FIntPoint& Cell)
	{
		return Cell == FIntPoint(19, 7) || Cell == FIntPoint(17, 9) || Cell == FIntPoint(17, 5);
	});
	if (TestTrue(TEXT("Tie found"), Tie.IsSet()))
	{
		TestEqual(TEXT("Tie broken eastward"), *Tie, TArray<FIntPoint>{ FIntPoint(18, 7), FIntPoint(19, 7) });
	}

	TestFalse(TEXT("No walkable way to (2,9)"), Grid.FindPath(Map.Camp, [](const FIntPoint& Cell) { return Cell == FIntPoint(2, 9); }).IsSet());
	return true;
}

#endif
