// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraWaveLoader.h"

#include "Rules/CytraBalance.h"
#include "Rules/CytraJson.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraWaveSchedule.h"

using CytraJson::FReader;

namespace
{
	bool ParseGroup(const FReader& Reader, const FCytraBalance& Balance, const FCytraMapDef& Map, FCytraWaveGroup& Group)
	{
		// Interval and delay default to 1 s and 0 s, as in the prototype.
		if (!Reader.Name(TEXT("route"), Group.Route) || !Reader.Name(TEXT("enemy"), Group.Enemy)
			|| !Reader.Integer(TEXT("count"), Group.Count)
			|| !Reader.OptionalNumber(TEXT("interval"), Group.Interval)
			|| !Reader.OptionalNumber(TEXT("delay"), Group.Delay))
		{
			return false;
		}
		if (!Map.Routes.ContainsByPredicate([&Group](const FCytraRoute& Route) { return Route.Id == Group.Route; }))
		{
			return Reader.Fail(TEXT("route"), FString::Printf(TEXT("unknown route '%s' in map %s"), *Group.Route.ToString(), *Map.Id.ToString()));
		}
		if (!Balance.Enemies.Contains(Group.Enemy))
		{
			return Reader.Fail(TEXT("enemy"), FString::Printf(TEXT("unknown enemy '%s'"), *Group.Enemy.ToString()));
		}
		if (Group.Count < 1)
		{
			return Reader.Fail(TEXT("count"), TEXT("must be at least 1"));
		}
		if (Group.Interval <= 0.0 || Group.Delay < 0.0)
		{
			return Reader.Fail(nullptr, TEXT("interval must be positive and delay not negative"));
		}
		return true;
	}
}

bool FCytraWaveLoader::ParseJson(const FString& Json, const FCytraBalance& Balance, const FCytraMapDef& Map, FCytraWaveSchedule& OutSchedule, FString& OutError)
{
	OutSchedule = FCytraWaveSchedule();

	TSharedPtr<FJsonObject> Root;
	if (!CytraJson::ParseObject(Json, Root, OutError))
	{
		return false;
	}

	const FReader Reader(*Root, FString(), OutError);
	FName MapId;
	const TArray<TSharedPtr<FJsonValue>>* Waves = nullptr;
	if (!Reader.Name(TEXT("map"), MapId) || !Reader.Array(TEXT("waves"), Waves))
	{
		return false;
	}
	if (MapId != Map.Id)
	{
		return Reader.Fail(TEXT("map"), FString::Printf(TEXT("waves are for map %s, not %s"), *MapId.ToString(), *Map.Id.ToString()));
	}
	if (Waves->IsEmpty())
	{
		return Reader.Fail(TEXT("waves"), TEXT("must not be empty"));
	}

	for (int32 WaveIndex = 0; WaveIndex < Waves->Num(); ++WaveIndex)
	{
		const FString WavePath = FString::Printf(TEXT("waves[%d]"), WaveIndex);
		const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
		if (!(*Waves)[WaveIndex]->TryGetArray(Groups) || Groups->IsEmpty())
		{
			OutError = WavePath + TEXT(": must be a non-empty array of groups");
			return false;
		}

		TArray<FCytraWaveGroup>& Wave = OutSchedule.Waves.AddDefaulted_GetRef();
		for (int32 GroupIndex = 0; GroupIndex < Groups->Num(); ++GroupIndex)
		{
			const FString GroupPath = FString::Printf(TEXT("%s[%d]"), *WavePath, GroupIndex);
			const TSharedPtr<FJsonObject>* GroupObject = nullptr;
			if (!(*Groups)[GroupIndex]->TryGetObject(GroupObject))
			{
				OutError = GroupPath + TEXT(": must be an object");
				return false;
			}
			if (!ParseGroup(FReader(**GroupObject, GroupPath, OutError), Balance, Map, Wave.AddDefaulted_GetRef()))
			{
				return false;
			}
		}
	}

	OutSchedule.ComputeThreats(Balance);
	return true;
}

bool FCytraWaveLoader::LoadFromFile(const FString& Path, const FCytraBalance& Balance, const FCytraMapDef& Map, FCytraWaveSchedule& OutSchedule, FString& OutError)
{
	FString Json;
	if (!CytraJson::LoadFile(Path, Json, OutError))
	{
		return false;
	}
	if (!ParseJson(Json, Balance, Map, OutSchedule, OutError))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Path, *OutError);
		return false;
	}
	return true;
}
