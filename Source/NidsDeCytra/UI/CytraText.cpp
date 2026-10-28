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
		TEXT("Hud.Threat"),
		TEXT("Hud.ForecastRoute"),
		TEXT("Hud.ForecastEnemies"),
		TEXT("Hud.Pause"),
		TEXT("Hud.Resume"),
		TEXT("Hud.Speed"),
		TEXT("Hud.Restart"),
		TEXT("Hud.RestartConfirm"),
		TEXT("Message.Opened"),
		TEXT("Message.TowerBuilt"),
		TEXT("Message.TowerUpgraded"),
		TEXT("Message.Dismantled"),
		TEXT("Message.Leak"),
		TEXT("Message.WaveCleared"),
		TEXT("Message.Ordered"),
		TEXT("Message.Upgrade"),
		TEXT("Message.DismantleOrdered"),
		TEXT("Message.Waiting"),
		TEXT("Message.Recruited"),
		TEXT("Message.Dismissed"),
		TEXT("Message.EarlyLaunch"),
		TEXT("End.WonTitle"),
		TEXT("End.WonBody"),
		TEXT("End.LostTitle"),
		TEXT("End.LostBody"),
		TEXT("End.Progress"),
		TEXT("End.Stats"),
		TEXT("End.Again"),
		TEXT("Worker.Idle"),
		TEXT("Worker.Move"),
		TEXT("Worker.Work"),
		TEXT("Worker.Back"),
		TEXT("Worker.ToMine"),
		TEXT("Worker.Mining"),
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
		TEXT("Panel.MetaWith"),
		TEXT("Panel.Level"),
		TEXT("Panel.HighTowerTag"),
		TEXT("Panel.Damage"),
		TEXT("Panel.Range"),
		TEXT("Panel.RangeValue"),
		TEXT("Panel.Rate"),
		TEXT("Panel.RateValue"),
		TEXT("Panel.Invested"),
		TEXT("Panel.MaxLevel"),
		TEXT("Panel.Upgrade"),
		TEXT("Panel.UpgradeExtra"),
		TEXT("Panel.UpgradeOrdered"),
		TEXT("Panel.Dismantle"),
		TEXT("Panel.DismantleExtra"),
		TEXT("Panel.DismantleOrdered"),
		TEXT("Panel.Refund"),
		TEXT("Panel.MineMiners"),
		TEXT("Panel.MineIncome"),
		TEXT("Panel.MineIncomeValue"),
		TEXT("Panel.MineMined"),
		TEXT("Panel.MineFull"),
		TEXT("Panel.MinePlaceLeft"),
		TEXT("Panel.MineEmpty"),
		TEXT("Crew.Title"),
		TEXT("Crew.Count"),
		TEXT("Crew.Recruit"),
		TEXT("Crew.Spec"),
		TEXT("Crew.Dismiss"),
		TEXT("Crew.DismissConfirm"),
		TEXT("Refusal.CrewFull"),
		TEXT("Panel.MineState"),
		TEXT("Panel.MineStateComing"),
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

	FText Data(const TCHAR* Group, FName Id, const TCHAR* Field)
	{
		return Get(FString::Printf(TEXT("%s.%s.%s"), Group, *Id.ToString(), Field));
	}

	bool Has(const FString& Key)
	{
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TableId);
		FString Source;
		return Table.IsValid() && Table->GetSourceString(Key, Source);
	}
}
