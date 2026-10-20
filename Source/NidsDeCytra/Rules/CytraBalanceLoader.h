// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FCytraBalance;

/** Reads and validates the balance data file. On failure, OutError says why and OutBalance is left unspecified. */
struct NIDSDECYTRA_API FCytraBalanceLoader
{
	static bool ParseJson(const FString& Json, FCytraBalance& OutBalance, FString& OutError);
	static bool LoadFromFile(const FString& Path, FCytraBalance& OutBalance, FString& OutError);
};
