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
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Rules/CytraBalanceLoader.h"
#include "Rules/CytraWaveLoader.h"
#include "World/CytraBoard.h"
#include "World/CytraEnemy.h"
#include "World/CytraMarkers.h"
#include "World/CytraNest.h"
#include "World/CytraWorker.h"

#if !UE_BUILD_SHIPPING
namespace
{
	FAutoConsoleCommandWithWorld KillAllCommand(
		TEXT("Cytra.KillAll"),
		TEXT("Kills every enemy on the map (development only)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ACytraGameMode* GameMode = World ? World->GetAuthGameMode<ACytraGameMode>() : nullptr)
			{
				GameMode->KillAllEnemies();
			}
		}));
}
#endif

ACytraGameMode::ACytraGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	GameStateClass = ACytraGameState::StaticClass();
	PlayerControllerClass = ACytraPlayerController::StaticClass();
	DefaultPawnClass = ACytraCameraPawn::StaticClass();
}

void ACytraGameMode::StartPlay()
{
	SpawnFallbackLighting();
	if (GetOrCreateBoard())
	{
		CreateMatch();
	}
	Super::StartPlay();
	SyncView();
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

void ACytraGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Match)
	{
		return;
	}

	StepAccumulator += FMath::Min(static_cast<double>(DeltaSeconds), MaxFrameSeconds);
	while (StepAccumulator >= StepSeconds)
	{
		Match->Step(StepSeconds);
		StepAccumulator -= StepSeconds;
	}
	SyncView();
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

bool ACytraGameMode::CreateMatch()
{
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	FString Error;
	if (!FCytraBalanceLoader::LoadFromFile(FPaths::ProjectContentDir() / BalanceFile, Balance, Error)
		|| !FCytraWaveLoader::LoadFromFile(FPaths::ProjectContentDir() / WavesFile, Balance, Board->GetMap(), Waves, Error))
	{
		UE_LOG(LogCytra, Error, TEXT("Match: cannot load data: %s"), *Error);
		return false;
	}

	Match = MakeUnique<FCytraMatch>(Board->GetMap(), Balance, Waves);

	// Without looks the match still runs, with the default shape.
	if (!FCytraLooks::LoadFromFile(FPaths::ProjectContentDir() / LooksFile, Looks, Error))
	{
		UE_LOG(LogCytra, Error, TEXT("Match: cannot load looks: %s"), *Error);
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	const FIntPoint NestCell = Board->GetMap().Nest;
	const FVector NestLocation = Board->CellToWorld(NestCell) + FVector(0.0f, 0.0f, Board->GetTileHeight(Board->GetMap().Get(NestCell)));
	Nest = GetWorld()->SpawnActor<ACytraNest>(NestLocation, FRotator::ZeroRotator, Params);
	Markers = GetWorld()->SpawnActor<ACytraMarkers>(Params);
	StepAccumulator = 0.0;
	LastPhase = Match->GetPhase();
	LastWave = Match->GetWave();
	UE_LOG(LogCytra, Log, TEXT("Match: started on %s, %d waves, %d gold, %d eggs"),
		*Board->GetMap().Id.ToString(), Match->GetWaveCount(), Match->GetWallet().Gold, Match->GetEggs());
	return true;
}

void ACytraGameMode::LaunchWave()
{
	if (!Match)
	{
		return;
	}
	const FCytraLaunchResult Result = Match->LaunchWave();
	if (Result.bLaunched && Result.Bonus > 0)
	{
		UE_LOG(LogCytra, Log, TEXT("Match: wave launched early, +%d gold"), Result.Bonus);
	}
	SyncView();
}

FCytraOrderResult ACytraGameMode::Order(const FIntPoint& Cell, const FCytraOrderAction& Action)
{
	if (!Match)
	{
		return FCytraOrderResult();
	}
	const FCytraOrderResult Result = Match->Order(Cell, Action);
	if (Result.IsAccepted())
	{
		UE_LOG(LogCytra, Log, TEXT("Match: order %s at (%d,%d), -%d gold%s"), *Action.Id.ToString(), Cell.X, Cell.Y,
			Result.Check.Spec->Cost, Result.bWaiting ? TEXT(", waiting for a worker") : TEXT(""));
	}
	else
	{
		UE_LOG(LogCytra, Log, TEXT("Match: order %s at (%d,%d) refused (%d)"), *Action.Id.ToString(), Cell.X, Cell.Y, static_cast<int32>(Result.Check.Refusal));
	}
	bGridChanged = true;
	SyncView();
	return Result;
}

void ACytraGameMode::ShowSelection(const TOptional<FIntPoint>& Cell)
{
	if (Markers && Board)
	{
		Markers->ShowSelection(*Board, Cell);
	}
}

void ACytraGameMode::KillAllEnemies()
{
	if (!Match)
	{
		return;
	}
	TArray<int32> Ids;
	for (const FCytraEnemy& Enemy : Match->GetEnemies())
	{
		Ids.Add(Enemy.Id);
	}
	for (const int32 Id : Ids)
	{
		Match->DamageEnemy(Id, TNumericLimits<double>::Max());
	}
	UE_LOG(LogCytra, Log, TEXT("Match: killed %d enemies (cheat)"), Ids.Num());
	SyncView();
}

void ACytraGameMode::SyncView()
{
	if (!Match)
	{
		return;
	}
	LogEvents();
	if (bGridChanged)
	{
		bGridChanged = false;
		Board->SyncGrid(Match->GetGrid(), Match->GetJobs());
	}
	SyncEnemies();
	SyncWorkers();
	if (Markers)
	{
		Markers->ShowJobs(*Board, Match->GetJobs());
	}
	if (Nest)
	{
		Nest->SetEggs(Match->GetEggs(), ACytraBoard::CellSize);
	}
	if (ACytraGameState* CytraState = GetGameState<ACytraGameState>())
	{
		CytraState->UpdateFromMatch(*Match);
	}
}

void ACytraGameMode::SyncEnemies()
{
	const float RoadHeight = Board->GetTileHeight(ECytraTileType::Road);
	TSet<int32> Alive;
	for (const FCytraEnemy& Enemy : Match->GetEnemies())
	{
		if (Enemy.bDead)
		{
			continue;
		}
		Alive.Add(Enemy.Id);
		TObjectPtr<ACytraEnemy>& Actor = EnemyActors.FindOrAdd(Enemy.Id);
		if (!Actor)
		{
			Actor = SpawnEnemy(Enemy.Type);
		}
		if (Actor)
		{
			Actor->SetActorLocation(Board->GridToWorld(Enemy.Position) + FVector(0.0f, 0.0f, RoadHeight));
		}
	}

	for (auto It = EnemyActors.CreateIterator(); It; ++It)
	{
		if (!Alive.Contains(It.Key()))
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}

void ACytraGameMode::SyncWorkers()
{
	for (const FCytraWorker& Worker : Match->GetWorkers())
	{
		TObjectPtr<ACytraWorker>& Actor = WorkerActors.FindOrAdd(Worker.Id);
		if (!Actor)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = GetWorld()->SpawnActor<ACytraWorker>(Params);
			if (!Actor)
			{
				continue;
			}
			Actor->SetLook(Looks.WorkerLook(Worker.Role), ACytraBoard::CellSize);
		}
		// Offset by id as in the web prototype, so that workers on the same cell stay apart.
		const FVector2d Offset(((Worker.Id % 3) - 1) * 6.0 / 40.0, (Worker.Id % 2 ? -5.0 : 5.0) / 40.0);
		const FIntPoint Cell(FMath::FloorToInt32(Worker.Position.X + 0.5), FMath::FloorToInt32(Worker.Position.Y + 0.5));
		const float Height = Board->GetTileHeight(Board->GetMap().Get(Cell));
		Actor->SetActorLocation(Board->GridToWorld(Worker.Position + Offset) + FVector(0.0f, 0.0f, Height));
	}
}

ACytraEnemy* ACytraGameMode::SpawnEnemy(FName Type)
{
	if (!Looks.Enemies.Contains(Type) && !EnemiesWithoutLook.Contains(Type))
	{
		EnemiesWithoutLook.Add(Type);
		UE_LOG(LogCytra, Warning, TEXT("Match: no look for enemy %s in %s, using the default shape"), *Type.ToString(), *LooksFile);
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACytraEnemy* Actor = GetWorld()->SpawnActor<ACytraEnemy>(Params);
	if (Actor)
	{
		Actor->SetLook(Looks.EnemyLook(Type), ACytraBoard::CellSize);
	}
	return Actor;
}

void ACytraGameMode::LogEvents()
{
	for (const FCytraMatchEvent& Event : Match->ConsumeEvents())
	{
		switch (Event.Type)
		{
		case ECytraMatchEventType::Leak:
			UE_LOG(LogCytra, Log, TEXT("Match: a %s reached the nest, -%d eggs"), *Event.Enemy.ToString(), Event.Eggs);
			break;
		case ECytraMatchEventType::WaveCleared:
			UE_LOG(LogCytra, Log, TEXT("Match: wave %d cleared, +%d gold"), Event.Wave, Event.Gold);
			break;
		case ECytraMatchEventType::JobDone:
			UE_LOG(LogCytra, Log, TEXT("Match: job %s done at (%d,%d), %d slots opened"), *Event.Work.ToString(), Event.Cell.X, Event.Cell.Y, Event.OpenedSlots);
			bGridChanged = true;
			break;
		}
	}

	if (Match->GetWave() != LastWave)
	{
		LastWave = Match->GetWave();
		UE_LOG(LogCytra, Log, TEXT("Match: wave %d/%d launched"), LastWave, Match->GetWaveCount());
	}
	if (Match->GetPhase() != LastPhase)
	{
		LastPhase = Match->GetPhase();
		if (LastPhase == ECytraMatchPhase::Won || LastPhase == ECytraMatchPhase::Lost)
		{
			UE_LOG(LogCytra, Log, TEXT("Match: %s at wave %d, %d eggs left, %d kills, %d gold"),
				LastPhase == ECytraMatchPhase::Won ? TEXT("won") : TEXT("lost"),
				Match->GetWave(), Match->GetEggs(), Match->GetKills(), Match->GetWallet().Gold);
		}
	}
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
