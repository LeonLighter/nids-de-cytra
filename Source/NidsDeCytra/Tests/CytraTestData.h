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

	/** Absolute path of the prototype balance, frozen under Source/NidsDeCytra/Tests/Reference. */
	FString ReferenceBalancePath();

	/**
	 * Loads the real map and the prototype balance (ReferenceBalancePath), so the rules tests keep
	 * checking the validated prototype while Content/Data/Balance.json is tuned. Failures are reported on Test.
	 */
	bool LoadMapAndBalance(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance);

	/** Loads the real map and the live balance, Content/Data/Balance.json. Failures are reported on Test. */
	bool LoadMapAndLiveBalance(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance);

	/** Loads the real map and wave files with the prototype balance. Failures are reported on Test. */
	bool LoadAll(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance, FCytraWaveSchedule& OutWaves);

	/** Reads a data file as a JSON object, to build broken variants of it. */
	TSharedPtr<FJsonObject> ReadJsonObject(const TCHAR* Relative);

	FString WriteJson(const TSharedPtr<FJsonObject>& Object);
}

#endif
