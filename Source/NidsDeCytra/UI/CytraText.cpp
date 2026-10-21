// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraText.h"

#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
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
		TEXT("Panel.Title"),
		TEXT("Panel.Hint"),
		TEXT("Panel.Hint1"),
		TEXT("Panel.Hint2"),
		TEXT("Panel.Hint3"),
		TEXT("Panel.Hint4"),
		TEXT("Panel.Cost"),
		TEXT("Panel.Meta"),
		TEXT("Panel.JobState"),
		TEXT("Panel.JobWaiting"),
		TEXT("Panel.JobComing"),
		TEXT("Panel.JobRunning"),
		TEXT("Panel.CutOff"),
		TEXT("Panel.ChainStep"),
		TEXT("Panel.HighTag"),
		TEXT("Refusal.NotAvailableHere"),
		TEXT("Refusal.GameOver"),
		TEXT("Refusal.JobInProgress"),
		TEXT("Refusal.Unreachable"),
		TEXT("Refusal.MissingSpecialty"),
		TEXT("Refusal.NotEnoughGold"),
		TEXT("Tile.Opened.Desc"),
		TEXT("Tile.OutpostSite.OpenedDesc"),
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

	FText Get(const FString& Key)
	{
		return FText::FromStringTable(TableId, Key);
	}

	bool Has(const FString& Key)
	{
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableId);
		FString Source;
		return Table.IsValid() && Table->GetSourceString(Key, Source);
	}
}
