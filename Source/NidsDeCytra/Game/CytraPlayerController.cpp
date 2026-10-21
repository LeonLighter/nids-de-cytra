// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraPlayerController.h"

#include "Engine/World.h"
#include "Game/CytraGameMode.h"
#include "UI/CytraHudWidget.h"

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

	if (IsLocalController())
	{
		Hud = CreateWidget<UCytraHudWidget>(this, UCytraHudWidget::StaticClass());
		if (Hud)
		{
			Hud->AddToViewport();
		}
	}
}

void ACytraPlayerController::LaunchWave()
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->LaunchWave();
	}
}
