// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FJsonObject;
struct FCytraBalance;
struct FCytraMapDef;
struct FCytraWaveSchedule;

/** Shared helpers for the rules tests. */
namespace CytraTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Absolute path of a file under Content/Data. */
	FString DataPath(const TCHAR* Relative);

	/** Loads the real map and balance files. Failures are reported on Test. */
	bool LoadMapAndBalance(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance);

	/** Loads the real map, balance and wave files. Failures are reported on Test. */
	bool LoadAll(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance, FCytraWaveSchedule& OutWaves);

	/** Reads a data file as a JSON object, to build broken variants of it. */
	TSharedPtr<FJsonObject> ReadJsonObject(const TCHAR* Relative);

	FString WriteJson(const TSharedPtr<FJsonObject>& Object);
}

#endif
