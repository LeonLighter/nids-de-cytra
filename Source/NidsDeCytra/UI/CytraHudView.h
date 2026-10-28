// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCytraMatch;

/** Texts of the top bar computed from the match, after updateUI() in the web prototype. Testable without any widget. */
struct NIDSDECYTRA_API FCytraHudView
{
	/** What a wave sends down each route, such as "Nord : 8 bandits · Sud : 2 aventuriers". WaveIndex counts from 0. */
	static FText Forecast(const FCytraMatch& Match, int32 WaveIndex);
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
