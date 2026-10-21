// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Game/CytraPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCytraInputControllerTicksTest, "Cytra.Game.Input.ControllerTicks", CytraTest::Flags)

bool FCytraInputControllerTicksTest::RunTest(const FString& Parameters)
{
	// Enhanced Input evaluates actions in PlayerTick: a controller that cannot tick never sees a click.
	TestTrue(TEXT("Player controller can tick"), GetDefault<ACytraPlayerController>()->PrimaryActorTick.bCanEverTick);
	return true;
}

#endif
