// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraCrew.h"
#include "Rules/CytraGrid.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMatchPhase.h"
#include "Rules/CytraRoutePath.h"
#include "Rules/CytraWallet.h"
#include "Rules/CytraWaveSchedule.h"

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
	/** Match time until which the enemy is slowed, and its speed multiplier meanwhile. */
	double SlowUntil = 0.0;
	double SlowFactor = 1.0;

	bool IsSlowed(double Time) const { return Time < SlowUntil; }
	/** Cells per second at match time Time. */
	double SpeedAt(double Time) const { return Speed * (IsSlowed(Time) ? SlowFactor : 1.0); }
};

/** Firing state of a tower, kept in order of construction: towers fire in that order, as in the prototype. */
struct FCytraTowerGun
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	/** Seconds before the next shot. Ready at 0 or below. */
	double Cooldown = 0.0;
	/** Direction of the last target, in radians from the +X axis of the grid. */
	double Aim = 0.0;
};

enum class ECytraShotKind : uint8
{
	/** Homing bolt, hits its target on contact. */
	Bolt,
	/** Shell flying to a fixed point, splash damage on landing. */
	Shell,
};

/** A projectile in flight. Positions are in grid cells. */
struct FCytraShot
{
	ECytraShotKind Kind = ECytraShotKind::Bolt;
	/** Type of the tower that fired it. */
	FName Tower;
	double Damage = 0.0;
	/** Bolt: current position, flight direction in radians, speed in cells per second and target enemy id. */
	FVector2d Position = FVector2d::ZeroVector;
	double Angle = 0.0;
	double Speed = 0.0;
	int32 Target = 0;
	/** Shell: launch and landing points, seconds in flight, flight duration and splash radius. */
	FVector2d From = FVector2d::ZeroVector;
	FVector2d To = FVector2d::ZeroVector;
	double Elapsed = 0.0;
	double Duration = 0.0;
	double Splash = 0.0;
	bool bDone = false;
};

enum class ECytraEffectKind : uint8
{
	/** Instant hit of a chain tower, from the tower to one enemy. */
	Zap,
	/** A shell landed. */
	Blast,
	/** An enemy died at To and paid Gold. */
	Loot,
	/** A miner extracted Gold at the mine on To. */
	MineGold,
	/** An enemy reached the nest on To. */
	Leak,
};

/** Something brief to show. The rules forget it at once. */
struct FCytraEffect
{
	ECytraEffectKind Kind = ECytraEffectKind::Zap;
	FName Tower;
	FVector2d From = FVector2d::ZeroVector;
	FVector2d To = FVector2d::ZeroVector;
	double Radius = 0.0;
	int32 Gold = 0;
};

enum class ECytraMatchEventType : uint8
{
	/** An enemy reached the nest. */
	Leak,
	/** A wave other than the last was cleared. */
	WaveCleared,
	/** A worker finished a job. */
	JobDone,
	/** A miner extracted gold. */
	MineYield,
};

struct FCytraMatchEvent
{
	ECytraMatchEventType Type = ECytraMatchEventType::Leak;
	/** Leak: the enemy type. */
	FName Enemy;
	/** Leak: eggs destroyed. */
	int32 Eggs = 0;
	/** WaveCleared: the wave number (1-based) and the bonus earned. MineYield: the gold extracted, at the mine on Cell. */
	int32 Wave = 0;
	int32 Gold = 0;
	/** JobDone: the job kind, its work id or tower type, its cell, and the tower slots it gave access to. */
	ECytraOrderKind Job = ECytraOrderKind::Work;
	FName Work;
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 OpenedSlots = 0;
	/** JobDone on a tower: its level once built or upgraded, the gold given back by a dismantling. */
	int32 Level = 0;
	int32 Refund = 0;
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

	/** Whether the order is accepted now, or the first reason it is refused. */
	FCytraOrderCheck CheckOrder(const FIntPoint& Cell, const FCytraOrderAction& Action) const;

	/** Pays for the order and queues its job, which takes the nearest free worker of its specialty. */
	FCytraOrderResult Order(const FIntPoint& Cell, const FCytraOrderAction& Action);

	/** Whether a worker of this specialty can be recruited now, or the first reason it cannot (GDD: crew full, then gold). */
	FCytraRecruitCheck CheckRecruit(FName Role) const;

	/** Pays for a recruit, who arrives at the main camp and may take a waiting job at once. */
	FCytraRecruitCheck Recruit(FName Role);

	/** Events since the last call, oldest first. */
	TArray<FCytraMatchEvent> ConsumeEvents();

	/** Effects since the last call, oldest first. */
	TArray<FCytraEffect> ConsumeEffects();

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

	/** Miners at the mine or on their way: the places taken. */
	int32 MinersAtMine() const;
	/** Miners extracting gold now. */
	int32 MinersMining() const;
	double GetTime() const { return Time; }

	const FCytraGrid& GetGrid() const { return Grid; }
	const TArray<FCytraWorker>& GetWorkers() const { return Workers; }
	/** Jobs in order of command. */
	const TArray<FCytraJob>& GetJobs() const { return Jobs; }
	const FCytraJob* FindJobAt(const FIntPoint& Cell) const;
	const FCytraWorker* FindWorker(int32 WorkerId) const;
	/** Towers in service, in order of construction. Type and level are on the grid. */
	const TArray<FCytraTowerGun>& GetTowers() const { return Towers; }
	/** Projectiles in flight. */
	const TArray<FCytraShot>& GetShots() const { return Shots; }

	const FCytraMapDef& GetMap() const { return Map; }
	const FCytraBalance& GetBalance() const { return Balance; }
	const FCytraWaveSchedule& GetWaves() const { return Waves; }

private:
	FCytraLaunchResult StartWave(bool bAuto);
	void Spawn(const FCytraSpawn& Entry);
	void MoveEnemies(double Dt);
	/** Every ready tower fires at the most advanced enemy in range, in order of construction. */
	void FireTowers(double Dt);
	void MoveShots(double Dt);
	/** Hits an enemy. At 0 hit points it dies and pays its gold. Dead enemies are ignored. */
	void Hit(FCytraEnemy& Enemy, double Damage);

	void AddWorker(FName Role);
	void StepWorkers(double Dt);
	/** Gives waiting jobs a worker, sends idle miners to the mine and everyone else back to the nearest camp. */
	void Manage();
	/** Starts a worker on Path. A worker between two cells first walks back to the centre of its own. */
	void Go(FCytraWorker& Worker, TArray<FIntPoint> Path, ECytraWorkerState State) const;
	/** Shortest path from the cell a worker stands on. */
	TOptional<TArray<FIntPoint>> PathFrom(const FCytraWorker& Worker, TFunctionRef<bool(const FIntPoint&)> Goal) const;
	void Finish(int32 JobIndex, FCytraWorker& Worker);
	FCytraJob* FindJob(int32 JobId);

	FCytraMapDef Map;
	FCytraBalance Balance;
	FCytraWaveSchedule Waves;
	TArray<FCytraRoutePath> Routes;
	/** Specialty that opens the mine, and the only one that works it. */
	FName MineRole;

	FCytraGrid Grid;
	TArray<FCytraWorker> Workers;
	TArray<FCytraJob> Jobs;
	TArray<FCytraTowerGun> Towers;
	TArray<FCytraShot> Shots;
	TArray<FCytraEffect> Effects;

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
	/** Shared by workers, jobs and enemies, as in the prototype. */
	int32 NextId = 0;
};
