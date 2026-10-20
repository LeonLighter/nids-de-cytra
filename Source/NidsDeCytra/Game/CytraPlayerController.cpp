// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraPlayerController.h"

ACytraPlayerController::ACytraPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ACytraPlayerController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}
