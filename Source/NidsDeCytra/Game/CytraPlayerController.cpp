// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraPlayerController.h"

#include "CytraLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/CytraGameMode.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/CytraHudWidget.h"
#include "World/CytraBoard.h"

ACytraPlayerController::ACytraPlayerController()
{
	// The controller must tick: Enhanced Input evaluates its actions in PlayerTick.
	PrimaryActorTick.bCanEverTick = true;
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

	CreateInputObjects();
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Input->AddMappingContext(InputContext, 0);
	}
	else
	{
		UE_LOG(LogCytra, Warning, TEXT("Input: no Enhanced Input subsystem, clicks will be ignored"));
	}

	if (IsLocalController())
	{
		Hud = CreateWidget<UCytraHudWidget>(this, UCytraHudWidget::StaticClass());
		if (Hud)
		{
			Hud->AddToViewport();
		}
	}
}

void ACytraPlayerController::CreateInputObjects()
{
	if (!ClickAction)
	{
		ClickAction = NewObject<UInputAction>(this, TEXT("IA_Select"));
		ClickAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!InputContext)
	{
		InputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Cytra"));
		InputContext->MapKey(ClickAction, EKeys::LeftMouseButton);
	}
}

void ACytraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInputObjects();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(ClickAction, ETriggerEvent::Started, this, &ACytraPlayerController::HandleClick);
	}
	else
	{
		UE_LOG(LogCytra, Warning, TEXT("Input: the input component is not an Enhanced Input component, clicks will be ignored"));
	}
}

void ACytraPlayerController::HandleClick()
{
	ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>();
	const ACytraBoard* Board = GameMode ? GameMode->GetBoard() : nullptr;
	FVector Origin;
	FVector Direction;
	if (!Board || !DeprojectMousePositionToWorld(Origin, Direction))
	{
		UE_LOG(LogCytra, Warning, TEXT("Input: click ignored, no board or no cursor position"));
		return;
	}
	if (const TOptional<FIntPoint> Cell = Board->CellUnderRay(Origin, Direction))
	{
		UE_LOG(LogCytra, Verbose, TEXT("Input: click on cell (%d,%d)"), Cell->X, Cell->Y);
		SelectCell(Cell);
	}
}

void ACytraPlayerController::SelectCell(const TOptional<FIntPoint>& Cell)
{
	if (Cell == SelectedCell)
	{
		return;
	}
	SelectedCell = Cell;
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->ShowSelection(SelectedCell);
	}
	OnSelectionChanged.Broadcast();
}

void ACytraPlayerController::OrderOnSelection(const FCytraOrderAction& Action)
{
	ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>();
	if (GameMode && SelectedCell)
	{
		GameMode->Order(*SelectedCell, Action);
	}
}

void ACytraPlayerController::Recruit(FName Specialty)
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->Recruit(Specialty);
	}
}

void ACytraPlayerController::PreviewTower(FName Tower)
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->ShowPreviewTower(Tower);
	}
}

void ACytraPlayerController::LaunchWave()
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->LaunchWave();
	}
}
