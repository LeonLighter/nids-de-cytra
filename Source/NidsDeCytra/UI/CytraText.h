// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Player-facing texts, read from the string table Content/Data/Text/CytraText.csv so they stay translatable. */
namespace CytraText
{
	extern NIDSDECYTRA_API const FName TableId;

	/**
	 * Every fixed key the code reads, so tests can check the table has them all.
	 * Keys built from data ids (Role.<Id>.Short, Work.<Id>.Label, Tile.<Type>.Title...) are checked against the data files.
	 */
	extern NIDSDECYTRA_API const TCHAR* const Keys[];
	extern NIDSDECYTRA_API const int32 NumKeys;

	/** Loads the table. Called at module startup. */
	void Register();
	void Unregister();

	NIDSDECYTRA_API FText Get(const TCHAR* Key);
	NIDSDECYTRA_API FText Get(const FString& Key);

	/** Whether the table has Key: data ids without a text show up in tests rather than on screen. */
	NIDSDECYTRA_API bool Has(const FString& Key);
}
