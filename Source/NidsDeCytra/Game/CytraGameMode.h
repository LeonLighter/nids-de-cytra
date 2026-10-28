// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Rules/CytraMatch.h"
#include "UI/CytraMessages.h"
#include "World/CytraLooks.h"
#include "CytraGameMode.generated.h"

class ACytraBoard;
class ACytraEnemy;
class ACytraMarkers;
class ACytraNest;
class ACytraShots;
class ACytraTower;
class ACytraWorker;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCytraMessage, const FCytraMessage&);

/** Gold earned shown over the board for a moment, as the "+6" of the prototype. */
struct FCytraFloatingGold
{
	/** Where it was earned, in grid cells. */
	FVector2d At = FVector2d::ZeroVector;
	int32 Gold = 0;
	/** Game seconds since it appeared, and how long it stays. */
	float Age = 0.0f;
	float Duration = 0.0f;
};

/**
 * Runs the match: owns the rules simulation, advances it in fixed steps and relays player orders to it.
 * It is the only ticking actor: the simulation has to move on every frame.
 */
UCLASS()
class NIDSDECYTRA_API ACytraGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/** Simulation step, as in the web prototype. */
	static constexpr double StepSeconds = 1.0 / 60.0;

	/** Longest frame simulated at once, before the speed factor, as in the web prototype: a longer hitch slows the game down instead. */
	static constexpr double MaxFrameSeconds = 0.1;

	/** Fastest game speed: the speed button goes 1, 2, 3, then back to 1. */
	static constexpr int32 MaxSpeed = 3;

	/** URL option that carries the game speed over a restart. */
	static constexpr const TCHAR* SpeedOption = TEXT("CytraSpeed");

	ACytraGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The board placed in the level, or a new one spawned at the origin. Built on first access. */
	ACytraBoard* GetOrCreateBoard();

	/** Loads Looks.json on first call: the camera needs it before the match starts. */
	const FCytraLooks& EnsureLooks();

	/** Null until the board is built. */
	const ACytraBoard* GetBoard() const { return Board; }

	/** Null if the data files could not be loaded. */
	const FCytraMatch* GetMatch() const { return Match.Get(); }

	/** Placeholder appearance of enemies, workers and towers. */
	const FCytraLooks& GetLooks() const { return Looks; }

	/** Player order: launch the next wave now. */
	void LaunchWave();

	/** Player order on a cell: pays for it and queues its job, or reports why it is refused. */
	FCytraOrderResult Order(const FIntPoint& Cell, const FCytraOrderAction& Action);

	/** Player order: recruit a worker of this specialty, who arrives at the main camp. */
	FCytraRecruitCheck Recruit(FName Specialty);

	/** Player order: dismiss a worker for part of its recruitment cost. */
	FCytraDismissCheck Dismiss(int32 WorkerId);

	/** Frames the selected cell on the board, or hides the frame. On a slot, also shows the tower range. */
	void ShowSelection(const TOptional<FIntPoint>& Cell);

	/**
	 * Tower type the player points at in the order panel: an empty selected slot shows its range.
	 * NAME_None shows the range of the first tower type of the data. Reset when the selection changes.
	 */
	void ShowPreviewTower(FName Tower);

	/** Development cheat: kills every enemy on the map, paying their gold. */
	void KillAllEnemies();

	/**
	 * Pauses or resumes the match. The engine keeps running: the simulation is just not advanced,
	 * so orders, the interface and the board stay live, as in the prototype.
	 */
	void TogglePause();
	bool IsMatchPaused() const { return bMatchPaused; }

	/** Game speed 1, 2, 3, then 1 again: the simulation advances that many times faster. */
	void CycleSpeed();
	int32 GetSpeed() const { return Speed; }

	/** Starts a new match by reloading the level, at the same speed and not paused, as the prototype does. */
	void RestartMatch();

	/** Frames the cell under the cursor, or hides the frame. */
	void ShowHover(const TOptional<FIntPoint>& Cell);

	/** Gold earned recently, for the interface to write over the board. */
	const TArray<FCytraFloatingGold>& GetFloatingGold() const { return FloatingGold; }

	/** A brief message for the player: an order accepted or refused, a job done, an egg lost, a wave cleared. */
	FOnCytraMessage OnMessage;

protected:
	/** Balance data file, relative to the Content folder. */
	UPROPERTY(EditAnywhere, Category = "Cytra")
	FString BalanceFile = TEXT("Data/Balance.json");

	/** Wave data file, relative to the Content folder. */
	UPROPERTY(EditAnywhere, Category = "Cytra")
	FString WavesFile = TEXT("Data/Waves/Passe01.json");

	/** Placeholder appearance file, relative to the Content folder. */
	UPROPERTY(EditAnywhere, Category = "Cytra")
	FString LooksFile = TEXT("Data/Looks.json");

private:
	/** The level ships without lights: add a sun, an atmosphere and a sky light if it has none. */
	void SpawnFallbackLighting();

	/** Loads balance and waves and starts a match on the board's map. */
	bool CreateMatch();

	/** Pushes the match state to the game state and the world, and logs its events. */
	void SyncView();
	/** Logs the events of the match and turns them into messages. */
	void LogEvents();
	void Say(const FCytraMessage& Message);

	/** Starts the new effects and ages the running ones: gold for the interface, the rest drawn by the shots actor. */
	void SyncEffects();

	/** During preparation, blinking arrows at the start of the routes the next wave takes. */
	void SyncNextRoutes();

	/** Spawns, moves and removes enemy actors to match the simulation. */
	void SyncEnemies();
	ACytraEnemy* SpawnEnemy(FName Type);

	/** Spawns and moves worker actors to match the simulation. */
	void SyncWorkers();

	/** Shows the towers in service and those being built, which grow by a quarter of their size every quarter of the job. */
	void SyncTowers();
	ACytraTower* SpawnTower(const FIntPoint& Cell, FName Type);

	/** Range circle of the selected slot, redrawn only when it changes. */
	void SyncRange();

	UPROPERTY(Transient)
	TObjectPtr<ACytraBoard> Board;

	UPROPERTY(Transient)
	TObjectPtr<ACytraNest> Nest;

	UPROPERTY(Transient)
	TObjectPtr<ACytraMarkers> Markers;

	/** Enemy actors by simulation id. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ACytraEnemy>> EnemyActors;

	/** Worker actors by simulation id. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ACytraWorker>> WorkerActors;

	/** Tower actors by slot, built or being built. */
	UPROPERTY(Transient)
	TMap<FIntPoint, TObjectPtr<ACytraTower>> TowerActors;

	UPROPERTY(Transient)
	TObjectPtr<ACytraShots> Shots;

	TUniquePtr<FCytraMatch> Match;
	FCytraLooks Looks;
	bool bLooksLoaded = false;
	/** Enemy types already reported as having no look. */
	TSet<FName> EnemiesWithoutLook;

	/** Set when an access may have opened or the jobs changed: the board redraws its marks. */
	bool bGridChanged = true;

	TOptional<FIntPoint> SelectedCell;
	FName PreviewTower;
	/** Range circle on screen. */
	TOptional<FIntPoint> ShownRangeCell;
	double ShownRange = 0.0;
	bool bShownRangeDashed = false;

	/** Game seconds of the current frame, for the effects: speed included, 0 in pause or when the view syncs outside the frame update. */
	float FrameSeconds = 0.0f;

	bool bMatchPaused = false;
	int32 Speed = 1;

	TArray<FCytraFloatingGold> FloatingGold;

	/** Routes taken by the next wave, as indices into the map routes, and the wave they were computed for. */
	TArray<int32> NextRoutes;
	int32 NextRoutesWave = INDEX_NONE;

	/** Game time not simulated yet, less than one step. */
	double StepAccumulator = 0.0;

	ECytraMatchPhase LastPhase = ECytraMatchPhase::Prep;
	int32 LastWave = 0;
};
