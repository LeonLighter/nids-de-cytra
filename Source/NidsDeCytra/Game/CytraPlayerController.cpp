// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraPlayerController.h"

#include "CytraLog.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Game/CytraCameraPawn.h"
#include "Game/CytraGameMode.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
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
	if (!PanAction)
	{
		PanAction = NewObject<UInputAction>(this, TEXT("IA_Pan"));
		PanAction->ValueType = EInputActionValueType::Axis2D;
	}
	if (!ZoomAction)
	{
		ZoomAction = NewObject<UInputAction>(this, TEXT("IA_Zoom"));
		ZoomAction->ValueType = EInputActionValueType::Axis1D;
	}
	if (!InputContext)
	{
		InputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Cytra"));
		InputContext->MapKey(ClickAction, EKeys::LeftMouseButton);
		InputContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);

		// Windows names letter keys after the keyboard layout: Z and Q on a French keyboard sit where W and A are on an English one.
		// A key gives +1 on X; the swizzle turns it to Y for up and down, the negation to -1 for left and down.
		const auto MapPan = [this](const FKey& Key, bool bVertical, bool bNegative)
		{
			FEnhancedActionKeyMapping& Mapping = InputContext->MapKey(PanAction, Key);
			if (bVertical)
			{
				Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(InputContext));
			}
			if (bNegative)
			{
				Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputContext));
			}
		};
		MapPan(EKeys::Z, true, false);
		MapPan(EKeys::W, true, false);
		MapPan(EKeys::S, true, true);
		MapPan(EKeys::Q, false, true);
		MapPan(EKeys::A, false, true);
		MapPan(EKeys::D, false, false);
	}
}

void ACytraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	CreateInputObjects();
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(ClickAction, ETriggerEvent::Started, this, &ACytraPlayerController::HandleClick);
		Input->BindAction(PanAction, ETriggerEvent::Triggered, this, &ACytraPlayerController::HandlePan);
		Input->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ACytraPlayerController::HandleZoom);
	}
	else
	{
		UE_LOG(LogCytra, Warning, TEXT("Input: the input component is not an Enhanced Input component, clicks will be ignored"));
	}
}

TOptional<FIntPoint> ACytraPlayerController::CellUnderCursor() const
{
	const ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>();
	const ACytraBoard* Board = GameMode ? GameMode->GetBoard() : nullptr;
	FVector Origin;
	FVector Direction;
	if (!Board || !DeprojectMousePositionToWorld(Origin, Direction))
	{
		return {};
	}
	return Board->CellUnderRay(Origin, Direction);
}

void ACytraPlayerController::HandleClick()
{
	if (const TOptional<FIntPoint> Cell = CellUnderCursor())
	{
		UE_LOG(LogCytra, Verbose, TEXT("Input: click on cell (%d,%d)"), Cell->X, Cell->Y);
		SelectCell(Cell);
	}
}

void ACytraPlayerController::HandlePan(const FInputActionValue& Value)
{
	if (ACytraCameraPawn* CameraPawn = GetPawn<ACytraCameraPawn>())
	{
		CameraPawn->Pan(Value.Get<FVector2D>(), GetWorld()->GetDeltaSeconds());
	}
}

void ACytraPlayerController::HandleZoom(const FInputActionValue& Value)
{
	ACytraCameraPawn* CameraPawn = GetPawn<ACytraCameraPawn>();
	FVector2D Cursor;
	if (CameraPawn && GetMousePosition(Cursor.X, Cursor.Y))
	{
		CameraPawn->ZoomAt(Value.Get<float>(), Cursor);
	}
}

void ACytraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	const TOptional<FIntPoint> Cell = CellUnderCursor();
	if (Cell != HoveredCell)
	{
		HoveredCell = Cell;
		if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
		{
			GameMode->ShowHover(HoveredCell);
		}
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

void ACytraPlayerController::TogglePause()
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->TogglePause();
	}
}

void ACytraPlayerController::CycleSpeed()
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->CycleSpeed();
	}
}

void ACytraPlayerController::RestartMatch()
{
	if (ACytraGameMode* GameMode = GetWorld()->GetAuthGameMode<ACytraGameMode>())
	{
		GameMode->RestartMatch();
	}
}
