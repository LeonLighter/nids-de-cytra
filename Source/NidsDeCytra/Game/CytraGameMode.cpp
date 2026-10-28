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
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Rules/CytraBalanceLoader.h"
#include "Rules/CytraWaveLoader.h"
#include "World/CytraBoard.h"
#include "World/CytraEnemy.h"
#include "World/CytraMarkers.h"
#include "World/CytraNest.h"
#include "World/CytraShots.h"
#include "World/CytraTower.h"
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

void ACytraGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	Speed = FMath::Clamp(UGameplayStatics::GetIntOption(Options, SpeedOption, 1), 1, MaxSpeed);
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
		CameraPawn->SetCameraLook(EnsureLooks().Camera);
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

	// As in the prototype: the frame is capped first, then multiplied by the speed.
	const double GameSeconds = bMatchPaused ? 0.0 : FMath::Min(static_cast<double>(DeltaSeconds), MaxFrameSeconds) * Speed;
	StepAccumulator += GameSeconds;
	while (StepAccumulator >= StepSeconds)
	{
		Match->Step(StepSeconds);
		StepAccumulator -= StepSeconds;
	}
	FrameSeconds = static_cast<float>(GameSeconds);
	SyncView();
	FrameSeconds = 0.0f;
	SyncNextRoutes();
}

void ACytraGameMode::TogglePause()
{
	bMatchPaused = !bMatchPaused;
	UE_LOG(LogCytra, Log, TEXT("Match: %s"), bMatchPaused ? TEXT("paused") : TEXT("resumed"));
	SyncView();
}

void ACytraGameMode::CycleSpeed()
{
	Speed = Speed % MaxSpeed + 1;
	UE_LOG(LogCytra, Log, TEXT("Match: speed x%d"), Speed);
	SyncView();
}

void ACytraGameMode::RestartMatch()
{
	const FString Level = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());
	UE_LOG(LogCytra, Log, TEXT("Match: restarting %s at speed x%d"), *Level, Speed);
	UGameplayStatics::OpenLevel(this, FName(*Level), true, FString::Printf(TEXT("%s=%d"), SpeedOption, Speed));
}

void ACytraGameMode::ShowHover(const TOptional<FIntPoint>& Cell)
{
	if (Markers && Board)
	{
		Markers->ShowHover(*Board, Cell);
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

const FCytraLooks& ACytraGameMode::EnsureLooks()
{
	if (!bLooksLoaded)
	{
		bLooksLoaded = true;
		// Without looks the match still runs, with the default shapes and camera.
		FString Error;
		if (!FCytraLooks::LoadFromFile(FPaths::ProjectContentDir() / LooksFile, Looks, Error))
		{
			Looks = FCytraLooks();
			UE_LOG(LogCytra, Error, TEXT("Match: cannot load looks: %s"), *Error);
		}
	}
	return Looks;
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

	EnsureLooks();

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	const FIntPoint NestCell = Board->GetMap().Nest;
	const FVector NestLocation = Board->CellToWorld(NestCell) + FVector(0.0f, 0.0f, Board->GetTileHeight(Board->GetMap().Get(NestCell)));
	Nest = GetWorld()->SpawnActor<ACytraNest>(NestLocation, FRotator::ZeroRotator, Params);
	Markers = GetWorld()->SpawnActor<ACytraMarkers>(Params);
	Shots = GetWorld()->SpawnActor<ACytraShots>(Params);
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
	if (const TOptional<FCytraMessage> Message = FCytraMessages::FromLaunch(Result))
	{
		Say(*Message);
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
	Say(FCytraMessages::FromOrder(Result));
	SyncView();
	return Result;
}

FCytraRecruitCheck ACytraGameMode::Recruit(FName Specialty)
{
	if (!Match)
	{
		return FCytraRecruitCheck();
	}
	const FCytraRecruitCheck Result = Match->Recruit(Specialty);
	if (Result.IsAccepted())
	{
		UE_LOG(LogCytra, Log, TEXT("Match: %s recruited, -%d gold, crew of %d"), *Specialty.ToString(), Result.Cost, Match->GetWorkers().Num());
	}
	else
	{
		UE_LOG(LogCytra, Log, TEXT("Match: recruiting %s refused (%d)"), *Specialty.ToString(), static_cast<int32>(Result.Refusal));
	}
	Say(FCytraMessages::FromRecruit(Specialty, Result));
	SyncView();
	return Result;
}

void ACytraGameMode::ShowSelection(const TOptional<FIntPoint>& Cell)
{
	SelectedCell = Cell;
	PreviewTower = NAME_None;
	if (Markers && Board)
	{
		Markers->ShowSelection(*Board, Cell);
	}
	SyncRange();
}

void ACytraGameMode::ShowPreviewTower(FName Tower)
{
	PreviewTower = Tower;
	SyncRange();
}

void ACytraGameMode::SyncRange()
{
	if (!Match || !Markers || !Board)
	{
		return;
	}
	TOptional<FIntPoint> Cell;
	double Range = 0.0;
	bool bDashed = false;
	const FCytraGrid& Grid = Match->GetGrid();
	const FCytraBalance& Balance = Match->GetBalance();
	if (SelectedCell && Grid.GetMap().IsInside(*SelectedCell) && IsCytraSlot(Grid.GetType(*SelectedCell)) && !Balance.Towers.IsEmpty())
	{
		Cell = SelectedCell;
		const bool bHighSlot = Grid.GetType(*SelectedCell) == ECytraTileType::HighSlot;
		if (const FCytraTower* Tower = Grid.GetTower(*SelectedCell))
		{
			Range = Balance.TowerStats(Tower->Type, Tower->Level, bHighSlot).Range;
		}
		else
		{
			// As in the prototype: the tower pointed at in the panel, else the first one, even while another is being built.
			const FName Type = Balance.Towers.Contains(PreviewTower) ? PreviewTower : Balance.Towers.CreateConstIterator()->Key;
			Range = Balance.TowerStats(Type, 1, bHighSlot).Range;
			bDashed = true;
		}
	}
	if (Cell == ShownRangeCell && Range == ShownRange && bDashed == bShownRangeDashed)
	{
		return;
	}
	ShownRangeCell = Cell;
	ShownRange = Range;
	bShownRangeDashed = bDashed;
	Markers->ShowRange(*Board, Cell, Range, bDashed);
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
	SyncTowers();
	SyncEffects();
	SyncRange();
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
		CytraState->UpdateFromMatch(*Match, bMatchPaused, Speed);
	}
}

void ACytraGameMode::SyncEffects()
{
	// Gold floats up for 0.7 s after a kill and 0.9 s at the mine, as in the prototype.
	constexpr float LootSeconds = 0.7f;
	constexpr float MineGoldSeconds = 0.9f;

	for (FCytraFloatingGold& Gold : FloatingGold)
	{
		Gold.Age += FrameSeconds;
	}
	FloatingGold.RemoveAll([](const FCytraFloatingGold& Gold) { return Gold.Age >= Gold.Duration; });

	TArray<FCytraEffect> Effects = Match->ConsumeEffects();
	Effects.RemoveAll([this](const FCytraEffect& Effect)
	{
		if (Effect.Kind != ECytraEffectKind::Loot && Effect.Kind != ECytraEffectKind::MineGold)
		{
			return false;
		}
		FloatingGold.Add(FCytraFloatingGold{ Effect.To, Effect.Gold, 0.0f, Effect.Kind == ECytraEffectKind::Loot ? LootSeconds : MineGoldSeconds });
		return true;
	});
	if (Shots)
	{
		Shots->Show(*Board, Match->GetShots(), MoveTemp(Effects), FrameSeconds);
	}
}

void ACytraGameMode::SyncNextRoutes()
{
	if (!Match || !Markers || !Board)
	{
		return;
	}
	const bool bShow = Match->GetPhase() == ECytraMatchPhase::Prep && Match->GetWave() < Match->GetWaveCount();
	if (bShow && NextRoutesWave != Match->GetWave())
	{
		NextRoutesWave = Match->GetWave();
		NextRoutes.Reset();
		const TArray<FCytraRoute>& Routes = Match->GetMap().Routes;
		TArray<FName> Order;
		for (const FCytraRoute& Route : Routes)
		{
			Order.Add(Route.Id);
		}
		for (const FCytraRouteForecast& Forecast : Match->GetWaves().Forecast(NextRoutesWave, Order))
		{
			NextRoutes.Add(Routes.IndexOfByPredicate([&Forecast](const FCytraRoute& Route) { return Route.Id == Forecast.Route; }));
		}
	}
	// The blink follows real time, so it goes on in pause, as in the prototype.
	const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(GetWorld()->GetRealTimeSeconds()) * 4.0f);
	Markers->ShowNextRoutes(*Board, bShow ? NextRoutes : TArray<int32>(), Pulse);
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
			Actor->ShowStatus(Enemy.MaxHp > 0 ? static_cast<float>(Enemy.Hp / Enemy.MaxHp) : 1.0f, Enemy.IsSlowed(Match->GetTime()));
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

void ACytraGameMode::SyncTowers()
{
	struct FShown
	{
		FName Type;
		int32 Level = 0;
		double Aim = 0.0;
		float Scale = 1.0f;
	};
	TMap<FIntPoint, FShown> Wanted;
	for (const FCytraTowerGun& Gun : Match->GetTowers())
	{
		if (const FCytraTower* Tower = Match->GetGrid().GetTower(Gun.Cell))
		{
			Wanted.Add(Gun.Cell, FShown{ Tower->Type, Tower->Level, Gun.Aim, 1.0f });
		}
	}
	for (const FCytraJob& Job : Match->GetJobs())
	{
		if (Job.Kind == ECytraOrderKind::Build)
		{
			// A quarter of the size at the order, then a quarter more for every quarter of the job done.
			const double Done = Job.Time > 0.0 ? Job.Progress / Job.Time : 1.0;
			const float Scale = 0.25f * FMath::Min(4, 1 + FMath::FloorToInt32(Done * 4.0));
			Wanted.Add(Job.Cell, FShown{ Job.Type, 0, 0.0, Scale });
		}
	}

	for (auto It = TowerActors.CreateIterator(); It; ++It)
	{
		const FShown* Shown = Wanted.Find(It.Key());
		if (!Shown || !It.Value() || It.Value()->GetType() != Shown->Type)
		{
			if (It.Value())
			{
				It.Value()->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	for (const TPair<FIntPoint, FShown>& Entry : Wanted)
	{
		TObjectPtr<ACytraTower>& Actor = TowerActors.FindOrAdd(Entry.Key);
		if (!Actor)
		{
			Actor = SpawnTower(Entry.Key, Entry.Value.Type);
		}
		if (Actor)
		{
			// Grid angle to world yaw: the board maps grid axes onto world axes.
			const FVector2d Direction(FMath::Cos(Entry.Value.Aim), FMath::Sin(Entry.Value.Aim));
			const FVector World = Board->GridToWorld(Direction) - Board->GridToWorld(FVector2d::ZeroVector);
			Actor->ShowState(Entry.Value.Level, static_cast<float>(World.Rotation().Yaw), Entry.Value.Scale);
		}
	}
}

ACytraTower* ACytraGameMode::SpawnTower(const FIntPoint& Cell, FName Type)
{
	if (!Looks.Towers.Contains(Type))
	{
		UE_LOG(LogCytra, Warning, TEXT("Match: no look for tower %s in %s, using the default shapes"), *Type.ToString(), *LooksFile);
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Location = Board->CellToWorld(Cell) + FVector(0.0f, 0.0f, Board->GetTileHeight(Board->GetMap().Get(Cell)));
	ACytraTower* Actor = GetWorld()->SpawnActor<ACytraTower>(Location, FRotator::ZeroRotator, Params);
	if (Actor)
	{
		Actor->SetTowerLook(Type, Looks.TowerLook(Type), ACytraBoard::CellSize);
	}
	return Actor;
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

void ACytraGameMode::Say(const FCytraMessage& Message)
{
	OnMessage.Broadcast(Message);
}

void ACytraGameMode::LogEvents()
{
	for (const FCytraMatchEvent& Event : Match->ConsumeEvents())
	{
		if (const TOptional<FCytraMessage> Message = FCytraMessages::FromEvent(Event))
		{
			Say(*Message);
		}
		switch (Event.Type)
		{
		case ECytraMatchEventType::Leak:
			UE_LOG(LogCytra, Log, TEXT("Match: a %s reached the nest, -%d eggs"), *Event.Enemy.ToString(), Event.Eggs);
			break;
		case ECytraMatchEventType::WaveCleared:
			UE_LOG(LogCytra, Log, TEXT("Match: wave %d cleared, +%d gold"), Event.Wave, Event.Gold);
			break;
		case ECytraMatchEventType::JobDone:
			switch (Event.Job)
			{
			case ECytraOrderKind::Work:
				UE_LOG(LogCytra, Log, TEXT("Match: job %s done at (%d,%d), %d slots opened"), *Event.Work.ToString(), Event.Cell.X, Event.Cell.Y, Event.OpenedSlots);
				break;
			case ECytraOrderKind::Build:
			case ECytraOrderKind::Upgrade:
				UE_LOG(LogCytra, Log, TEXT("Match: %s at (%d,%d) in service at level %d"), *Event.Work.ToString(), Event.Cell.X, Event.Cell.Y, Event.Level);
				break;
			case ECytraOrderKind::Dismantle:
				UE_LOG(LogCytra, Log, TEXT("Match: %s at (%d,%d) dismantled, %d gold back"), *Event.Work.ToString(), Event.Cell.X, Event.Cell.Y, Event.Refund);
				break;
			}
			bGridChanged = true;
			break;
		case ECytraMatchEventType::MineYield:
			UE_LOG(LogCytra, Verbose, TEXT("Match: +%d gold from the mine, %d in total"), Event.Gold, Match->GetWallet().Mined);
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
