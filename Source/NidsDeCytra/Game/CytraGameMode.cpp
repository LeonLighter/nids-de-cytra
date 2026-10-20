// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraGameMode.h"

#include "Game/CytraCameraPawn.h"
#include "Game/CytraGameState.h"
#include "Game/CytraPlayerController.h"

ACytraGameMode::ACytraGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	GameStateClass = ACytraGameState::StaticClass();
	PlayerControllerClass = ACytraPlayerController::StaticClass();
	DefaultPawnClass = ACytraCameraPawn::StaticClass();
}
