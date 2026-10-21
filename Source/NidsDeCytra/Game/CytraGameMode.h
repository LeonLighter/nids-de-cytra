// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Rules/CytraMatch.h"
#include "World/CytraLooks.h"
#include "CytraGameMode.generated.h"

class ACytraBoard;
class ACytraEnemy;
class ACytraMarkers;
class ACytraNest;

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

	/** Longest frame simulated at once, as in the web prototype: a longer hitch slows the game down instead. */
	static constexpr double MaxFrameSeconds = 0.1;

	ACytraGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The board placed in the level, or a new one spawned at the origin. Built on first access. */
	ACytraBoard* GetOrCreateBoard();

	/** Null until the board is built. */
	const ACytraBoard* GetBoard() const { return Board; }

	/** Null if the data files could not be loaded. */
	const FCytraMatch* GetMatch() const { return Match.Get(); }

	/** Player order: launch the next wave now. */
	void LaunchWave();

	/** Frames the selected cell on the board, or hides the frame. */
	void ShowSelection(const TOptional<FIntPoint>& Cell);

	/** Development cheat: kills every enemy on the map, paying their gold. */
	void KillAllEnemies();

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
	void LogEvents();

	/** Spawns, moves and removes enemy actors to match the simulation. */
	void SyncEnemies();
	ACytraEnemy* SpawnEnemy(FName Type);

	UPROPERTY(Transient)
	TObjectPtr<ACytraBoard> Board;

	UPROPERTY(Transient)
	TObjectPtr<ACytraNest> Nest;

	UPROPERTY(Transient)
	TObjectPtr<ACytraMarkers> Markers;

	/** Enemy actors by simulation id. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<ACytraEnemy>> EnemyActors;

	TUniquePtr<FCytraMatch> Match;
	FCytraLooks Looks;
	/** Enemy types already reported as having no look. */
	TSet<FName> EnemiesWithoutLook;

	/** Game time not simulated yet, less than one step. */
	double StepAccumulator = 0.0;

	ECytraMatchPhase LastPhase = ECytraMatchPhase::Prep;
	int32 LastWave = 0;
};
