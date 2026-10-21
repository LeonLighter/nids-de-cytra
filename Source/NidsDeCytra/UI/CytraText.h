// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Player-facing texts, read from the string table Content/Data/Text/CytraText.csv so they stay translatable. */
namespace CytraText
{
	extern NIDSDECYTRA_API const FName TableId;

	/** Every key the code reads, so tests can check the table has them all. */
	extern NIDSDECYTRA_API const TCHAR* const Keys[];
	extern NIDSDECYTRA_API const int32 NumKeys;

	/** Loads the table. Called at module startup. */
	void Register();
	void Unregister();

	NIDSDECYTRA_API FText Get(const TCHAR* Key);
}
