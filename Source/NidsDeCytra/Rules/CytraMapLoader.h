// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCytraMapDef;

/** Reads and validates a map data file. On failure, OutError says why and OutMap is left unspecified. */
struct NIDSDECYTRA_API FCytraMapLoader
{
	static bool ParseJson(const FString& Json, FCytraMapDef& OutMap, FString& OutError);
	static bool LoadFromFile(const FString& Path, FCytraMapDef& OutMap, FString& OutError);
};
