// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCytraMatch;

/** Texts of the top bar computed from the match, after updateUI() in the web prototype. Testable without any widget. */
struct NIDSDECYTRA_API FCytraHudView
{
	/** What a wave sends down each route, such as "Nord : 8 bandits · Sud : 2 aventuriers". WaveIndex counts from 0. */
	static FText Forecast(const FCytraMatch& Match, int32 WaveIndex);

	/**
	 * Label of the launch button between waves. It shows the early launch bonus and the gold of the mine only when there
	 * is some: without a countdown, or when both are 0, it names the wave to launch (1-based NextWave).
	 */
	static FText LaunchLabel(bool bHasCountdown, int32 EarlyLaunchBonus, int32 MineGold, int32 NextWave);
};

/** Content of the end screen, after updateUI() in the prototype, with the wave and the eggs the GDD asks for. */
struct NIDSDECYTRA_API FCytraEndView
{
	bool bWon = false;
	FText Title;
	FText Body;
	/** Wave reached and eggs left. */
	FText Progress;
	/** Kills, gold spent, gold mined and towers in service. */
	FText Stats;

	/** Unset while the match goes on. */
	static TOptional<FCytraEndView> Build(const FCytraMatch& Match);
};
