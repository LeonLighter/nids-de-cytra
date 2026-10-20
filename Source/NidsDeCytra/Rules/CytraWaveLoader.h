// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCytraBalance;
struct FCytraMapDef;
struct FCytraWaveSchedule;

/**
 * Reads and validates a wave file. Enemies must exist in Balance and routes in Map.
 * On failure, OutError says why and OutSchedule is left unspecified.
 */
struct NIDSDECYTRA_API FCytraWaveLoader
{
	static bool ParseJson(const FString& Json, const FCytraBalance& Balance, const FCytraMapDef& Map, FCytraWaveSchedule& OutSchedule, FString& OutError);
	static bool LoadFromFile(const FString& Path, const FCytraBalance& Balance, const FCytraMapDef& Map, FCytraWaveSchedule& OutSchedule, FString& OutError);
};
