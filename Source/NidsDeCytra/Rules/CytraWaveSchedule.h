// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCytraBalance;

/** Count enemies of one type on one route: the first after Delay seconds, then one every Interval seconds. */
struct FCytraWaveGroup
{
	FName Route;
	FName Enemy;
	int32 Count = 0;
	double Interval = 1.0;
	double Delay = 0.0;
};

/** One enemy to spawn, Time seconds after the wave started. */
struct FCytraSpawn
{
	double Time = 0.0;
	FName Enemy;
	FName Route;
};

/** What a wave sends down one route: enemy type and count, in order of first appearance in the data. */
struct FCytraRouteForecast
{
	FName Route;
	TArray<TPair<FName, int32>> Enemies;
};

/**
 * The waves of a map, read from Content/Data/Waves. Waves are indexed from 0 here;
 * the balance formulas (hit points, end bonus) take the 1-based wave number used in the GDD.
 */
struct NIDSDECYTRA_API FCytraWaveSchedule
{
	TArray<TArray<FCytraWaveGroup>> Waves;

	int32 Num() const { return Waves.Num(); }

	/** Precomputes the threat of every wave. The loader calls it; call it again after editing Waves. */
	void ComputeThreats(const FCytraBalance& Balance);

	/** Total base hit points of a wave, scaled by its wave number. */
	double Threat(int32 WaveIndex) const { return Threats[WaveIndex]; }

	/** Displayed difficulty, from 1 to Balance.ThreatLevels. */
	int32 ThreatLevel(int32 WaveIndex, const FCytraBalance& Balance) const;

	/** Countdown, in whole seconds, before the wave launches on its own. */
	int32 PrepTime(int32 WaveIndex, const FCytraBalance& Balance) const;

	/** Every spawn of a wave, sorted by time. Spawns at the same time keep their data order. */
	TArray<FCytraSpawn> SpawnQueue(int32 WaveIndex) const;

	/** Enemies per route for the wave announcement, following RouteOrder and skipping unused routes. */
	TArray<FCytraRouteForecast> Forecast(int32 WaveIndex, TConstArrayView<FName> RouteOrder) const;

private:
	double ThreatRatio(int32 WaveIndex) const;

	TArray<double> Threats;
	double MaxThreat = 0.0;
};
