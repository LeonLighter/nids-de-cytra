// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraRoutePath.h"
#include "Rules/CytraWallet.h"
#include "Rules/CytraWaveSchedule.h"

enum class ECytraMatchPhase : uint8
{
	/** Between waves. Before wave 1 there is no countdown: the player launches it. */
	Prep,
	Wave,
	Won,
	Lost,
};

/** An enemy walking its route. Positions are in grid cells. */
struct FCytraEnemy
{
	int32 Id = 0;
	FName Type;
	/** Index into the map routes. */
	int32 Route = 0;
	/** Cells travelled from the spawn point. */
	double Distance = 0.0;
	FVector2d Position = FVector2d::ZeroVector;
	/** Fractional: tower damage is not a whole number. */
	double Hp = 0.0;
	int32 MaxHp = 0;
	double Speed = 0.0;
	int32 Gold = 0;
	int32 Eggs = 0;
	bool bDead = false;
};

enum class ECytraMatchEventType : uint8
{
	/** An enemy reached the nest. */
	Leak,
	/** A wave other than the last was cleared. */
	WaveCleared,
};

struct FCytraMatchEvent
{
	ECytraMatchEventType Type = ECytraMatchEventType::Leak;
	/** Leak: the enemy type. */
	FName Enemy;
	/** Leak: eggs destroyed. */
	int32 Eggs = 0;
	/** WaveCleared: the wave number (1-based) and the bonus earned. */
	int32 Wave = 0;
	int32 Gold = 0;
};

struct FCytraLaunchResult
{
	bool bLaunched = false;
	/** Early launch bonus paid. */
	int32 Bonus = 0;
};

/**
 * One match, as a deterministic simulation advanced by Step. Mirrors step() in the web prototype:
 * same order of updates, same formulas, doubles everywhere so results match its JavaScript numbers.
 */
class NIDSDECYTRA_API FCytraMatch
{
public:
	FCytraMatch(const FCytraMapDef& InMap, const FCytraBalance& InBalance, const FCytraWaveSchedule& InWaves);

	/** Advances the match by Dt seconds. The game uses fixed steps of 1/60 s. Does nothing once the match is over. */
	void Step(double Dt);

	/** Launches the next wave now, paying the early launch bonus if a countdown was running. Refused outside preparation. */
	FCytraLaunchResult LaunchWave();

	/** Hits an enemy. At 0 hit points it dies and pays its gold. Unknown or dead enemies are ignored. */
	void DamageEnemy(int32 EnemyId, double Damage);

	/** Events since the last call, oldest first. */
	TArray<FCytraMatchEvent> ConsumeEvents();

	ECytraMatchPhase GetPhase() const { return Phase; }
	bool IsOver() const { return Phase == ECytraMatchPhase::Won || Phase == ECytraMatchPhase::Lost; }
	const FCytraWallet& GetWallet() const { return Wallet; }
	int32 GetEggs() const { return Eggs; }
	/** Waves launched so far: during a wave, its 1-based number. */
	int32 GetWave() const { return Wave; }
	int32 GetWaveCount() const { return Waves.Num(); }
	/** Seconds before the next wave launches on its own. Unset before wave 1 and outside preparation. */
	TOptional<double> GetCountdown() const { return Countdown; }
	const TArray<FCytraEnemy>& GetEnemies() const { return Enemies; }
	/** Enemies of the current wave not spawned yet. */
	int32 GetPendingSpawns() const { return SpawnQueue.Num(); }
	int32 GetKills() const { return Kills; }
	double GetTime() const { return Time; }

	const FCytraMapDef& GetMap() const { return Map; }
	const FCytraBalance& GetBalance() const { return Balance; }
	const FCytraWaveSchedule& GetWaves() const { return Waves; }

private:
	FCytraLaunchResult StartWave(bool bAuto);
	void Spawn(const FCytraSpawn& Entry);
	void MoveEnemies(double Dt);

	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	TArray<FCytraRoutePath> Routes;

	FCytraWallet Wallet;
	int32 Eggs = 0;
	int32 Wave = 0;
	ECytraMatchPhase Phase = ECytraMatchPhase::Prep;
	TOptional<double> Countdown;
	double Time = 0.0;
	/** Seconds since the current wave started. */
	double WaveTime = 0.0;
	/** Spawns of the current wave still to come, sorted by time. */
	TArray<FCytraSpawn> SpawnQueue;
	TArray<FCytraEnemy> Enemies;
	TArray<FCytraMatchEvent> Events;
	int32 Kills = 0;
	int32 NextId = 0;
};
