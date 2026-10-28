// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraHudView.h"

#include "Rules/CytraMatch.h"
#include "UI/CytraText.h"

namespace
{
	/**
	 * Picks the plural form of a count as the prototype does: singular up to 1, so "0 tourelle".
	 * The plural rules of FText follow the culture of the game, which may not be French.
	 */
	int32 PluralForm(int32 Count)
	{
		return Count > 1 ? 2 : 1;
	}
}

FText FCytraHudView::Forecast(const FCytraMatch& Match, int32 WaveIndex)
{
	if (WaveIndex < 0 || WaveIndex >= Match.GetWaveCount())
	{
		return FText::GetEmpty();
	}
	// Routes in map order, as the prototype lists the north pass first.
	TArray<FName> RouteOrder;
	for (const FCytraRoute& Route : Match.GetMap().Routes)
	{
		RouteOrder.Add(Route.Id);
	}
	TArray<FText> Parts;
	for (const FCytraRouteForecast& Forecast : Match.GetWaves().Forecast(WaveIndex, RouteOrder))
	{
		TArray<FText> Enemies;
		for (const TPair<FName, int32>& Entry : Forecast.Enemies)
		{
			const FText Name = CytraText::Data(TEXT("Enemy"), Entry.Key, Entry.Value > 1 ? TEXT("Many") : TEXT("One"));
			Enemies.Add(FText::Format(CytraText::Get(TEXT("Hud.ForecastEnemies")), Entry.Value, Name));
		}
		Parts.Add(FText::Format(CytraText::Get(TEXT("Hud.ForecastRoute")), CytraText::Data(TEXT("Route"), Forecast.Route, TEXT("Name")),
			FText::Join(INVTEXT(", "), Enemies)));
	}
	return FText::Join(INVTEXT(" · "), Parts);
}

TOptional<FCytraEndView> FCytraEndView::Build(const FCytraMatch& Match)
{
	if (!Match.IsOver())
	{
		return {};
	}
	FCytraEndView View;
	View.bWon = Match.GetPhase() == ECytraMatchPhase::Won;
	if (View.bWon)
	{
		View.Title = CytraText::Get(TEXT("End.WonTitle"));
		View.Body = FText::Format(CytraText::Get(TEXT("End.WonBody")), Match.GetEggs(), Match.GetBalance().StartEggs);
	}
	else
	{
		View.Title = CytraText::Get(TEXT("End.LostTitle"));
		View.Body = FText::Format(CytraText::Get(TEXT("End.LostBody")), Match.GetWave());
	}
	View.Progress = FText::Format(CytraText::Get(TEXT("End.Progress")), Match.GetWave(), Match.GetWaveCount(), Match.GetEggs(), PluralForm(Match.GetEggs()));
	const FCytraWallet& Wallet = Match.GetWallet();
	const int32 Towers = Match.GetTowers().Num();
	View.Stats = FText::Format(CytraText::Get(TEXT("End.Stats")),
		{ Match.GetKills(), PluralForm(Match.GetKills()), Wallet.Spent, Wallet.Mined, Towers, PluralForm(Towers) });
	return View;
}
