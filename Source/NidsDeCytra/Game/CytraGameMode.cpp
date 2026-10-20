// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraGameMode.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "CytraLog.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/CytraCameraPawn.h"
#include "Game/CytraGameState.h"
#include "Game/CytraPlayerController.h"
#include "GameFramework/Controller.h"
#include "World/CytraBoard.h"

ACytraGameMode::ACytraGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	GameStateClass = ACytraGameState::StaticClass();
	PlayerControllerClass = ACytraPlayerController::StaticClass();
	DefaultPawnClass = ACytraCameraPawn::StaticClass();
}

void ACytraGameMode::StartPlay()
{
	SpawnFallbackLighting();
	GetOrCreateBoard();
	Super::StartPlay();
}

void ACytraGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	ACytraCameraPawn* CameraPawn = NewPlayer ? Cast<ACytraCameraPawn>(NewPlayer->GetPawn()) : nullptr;
	ACytraBoard* CurrentBoard = GetOrCreateBoard();
	if (CameraPawn && CurrentBoard)
	{
		CameraPawn->FrameBounds(CurrentBoard->GetBoardBounds());
	}
}

ACytraBoard* ACytraGameMode::GetOrCreateBoard()
{
	if (!Board)
	{
		UWorld* World = GetWorld();
		TActorIterator<ACytraBoard> It(World);
		Board = It ? *It : World->SpawnActor<ACytraBoard>();
	}
	if (Board && !Board->EnsureBuilt())
	{
		return nullptr;
	}
	return Board;
}

void ACytraGameMode::SpawnFallbackLighting()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ADirectionalLight>(World))
	{
		return;
	}
	UE_LOG(LogCytra, Log, TEXT("No directional light in the level: spawning fallback lighting"));

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-55.0f, -30.0f, 0.0f), Params);
	UDirectionalLightComponent* SunComponent = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunComponent->SetMobility(EComponentMobility::Movable);
	SunComponent->SetAtmosphereSunLight(true);

	if (!TActorIterator<ASkyAtmosphere>(World))
	{
		World->SpawnActor<ASkyAtmosphere>(Params);
	}

	if (!TActorIterator<ASkyLight>(World))
	{
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(Params);
		USkyLightComponent* SkyComponent = Sky->GetLightComponent();
		SkyComponent->SetMobility(EComponentMobility::Movable);
		SkyComponent->bRealTimeCapture = true;
		SkyComponent->RecaptureSky();
	}
}
