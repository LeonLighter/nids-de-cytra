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

	uint32 Jobs = GetTypeHash(Match.GetJobs().Num());
	for (const FCytraJob& Job : Match.GetJobs())
	{
		const FCytraWorker* Worker = Match.FindWorker(Job.Worker);
		Jobs = HashCombineFast(Jobs, HashCombineFast(GetTypeHash(Job.Id), GetTypeHash(Job.Worker)));
		Jobs = HashCombineFast(Jobs, GetTypeHash(Worker ? static_cast<uint8>(Worker->State) : MAX_uint8));
	}
	Set(JobsSignature, Jobs);

	if (bChanged)
	{
		OnMatchChanged.Broadcast();
	}
}
