// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraGameState.h"

#include "Rules/CytraMatch.h"

ACytraGameState::ACytraGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACytraGameState::UpdateFromMatch(const FCytraMatch& Match)
{
	bool bChanged = false;
	auto Set = [&bChanged](auto& Field, const auto Value)
	{
		if (Field != Value)
		{
			Field = Value;
			bChanged = true;
		}
	};

	const TOptional<double> Countdown = Match.GetCountdown();
	Set(Gold, Match.GetWallet().Gold);
	Set(Eggs, Match.GetEggs());
	Set(Wave, Match.GetWave());
	Set(WaveCount, Match.GetWaveCount());
	Set(Phase, Match.GetPhase());
	Set(bHasCountdown, Countdown.IsSet());
	Set(CountdownSeconds, Countdown.IsSet() ? static_cast<int32>(FMath::CeilToDouble(Countdown.GetValue())) : 0);
	Set(EarlyLaunchBonus, Countdown.IsSet() ? Match.GetBalance().EarlyLaunchBonus(Countdown.GetValue()) : 0);
	Set(EnemiesLeft, Match.GetEnemies().Num() + Match.GetPendingSpawns());

	if (bChanged)
	{
		OnMatchChanged.Broadcast();
	}
}
