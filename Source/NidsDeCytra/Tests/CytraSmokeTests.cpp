// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraSmokeSanityTest, "Cytra.Smoke.Sanity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FCytraSmokeSanityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Arithmetic still works"), 1 + 1, 2);
	return true;
}

#endif
