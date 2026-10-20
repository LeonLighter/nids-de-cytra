// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraWaveSchedule.h"

#include "Algo/StableSort.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraMath.h"

void FCytraWaveSchedule::ComputeThreats(const FCytraBalance& Balance)
{
	Threats.Reset(Waves.Num());
	MaxThreat = 0.0;
	for (int32 Index = 0; Index < Waves.Num(); ++Index)
	{
		double Threat = 0.0;
		for (const FCytraWaveGroup& Group : Waves[Index])
		{
			Threat += Group.Count * Balance.Enemies.FindChecked(Group.Enemy).Hp * (1.0 + Balance.HpScalePerWave * Index);
		}
		Threats.Add(Threat);
		MaxThreat = FMath::Max(MaxThreat, Threat);
	}
}

double FCytraWaveSchedule::ThreatRatio(int32 WaveIndex) const
{
	return MaxThreat > 0.0 ? Threats[WaveIndex] / MaxThreat : 0.0;
}

int32 FCytraWaveSchedule::ThreatLevel(int32 WaveIndex, const FCytraBalance& Balance) const
{
	return FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(Balance.ThreatLevels * FMath::Sqrt(ThreatRatio(WaveIndex)))));
}

int32 FCytraWaveSchedule::PrepTime(int32 WaveIndex, const FCytraBalance& Balance) const
{
	return CytraMath::Round(Balance.PrepBase + Balance.PrepScale * FMath::Sqrt(ThreatRatio(WaveIndex)));
}

TArray<FCytraSpawn> FCytraWaveSchedule::SpawnQueue(int32 WaveIndex) const
{
	TArray<FCytraSpawn> Queue;
	for (const FCytraWaveGroup& Group : Waves[WaveIndex])
	{
		for (int32 i = 0; i < Group.Count; ++i)
		{
			Queue.Add({ Group.Delay + i * Group.Interval, Group.Enemy, Group.Route });
		}
	}
	Algo::StableSortBy(Queue, &FCytraSpawn::Time);
	return Queue;
}

TArray<FCytraRouteForecast> FCytraWaveSchedule::Forecast(int32 WaveIndex, TConstArrayView<FName> RouteOrder) const
{
	TArray<FCytraRouteForecast> Result;
	for (const FName Route : RouteOrder)
	{
		FCytraRouteForecast Forecast;
		Forecast.Route = Route;
		for (const FCytraWaveGroup& Group : Waves[WaveIndex])
		{
			if (Group.Route != Route)
			{
				continue;
			}
			TPair<FName, int32>* Entry = Forecast.Enemies.FindByPredicate([&Group](const TPair<FName, int32>& E) { return E.Key == Group.Enemy; });
			if (Entry)
			{
				Entry->Value += Group.Count;
			}
			else
			{
				Forecast.Enemies.Emplace(Group.Enemy, Group.Count);
			}
		}
		if (!Forecast.Enemies.IsEmpty())
		{
			Result.Add(MoveTemp(Forecast));
		}
	}
	return Result;
}
