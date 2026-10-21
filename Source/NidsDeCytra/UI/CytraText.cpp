// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraText.h"

#include "Internationalization/StringTableRegistry.h"
#include "Misc/Paths.h"

namespace CytraText
{
	const FName TableId(TEXT("CytraText"));

	const TCHAR* const Keys[] = {
		TEXT("Hud.Gold"),
		TEXT("Hud.Eggs"),
		TEXT("Hud.Wave"),
		TEXT("Hud.WaveValue"),
		TEXT("Hud.Next"),
		TEXT("Hud.NextIn"),
		TEXT("Hud.WaveRunning"),
		TEXT("Hud.Won"),
		TEXT("Hud.Lost"),
		TEXT("Hud.LaunchFirst"),
		TEXT("Hud.LaunchNow"),
		TEXT("Hud.WaveInProgress"),
		TEXT("Hud.GameOver"),
	};
	const int32 NumKeys = UE_ARRAY_COUNT(Keys);

	void Register()
	{
		LOCTABLE_FROMFILE_GAME("CytraText", "CytraText", "Data/Text/CytraText.csv");
	}

	void Unregister()
	{
		FStringTableRegistry::Get().UnregisterStringTable(TableId);
	}

	FText Get(const TCHAR* Key)
	{
		return FText::FromStringTable(TableId, Key);
	}
}
