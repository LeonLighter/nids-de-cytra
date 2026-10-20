// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/CytraTestData.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rules/CytraBalance.h"
#include "Rules/CytraBalanceLoader.h"
#include "Rules/CytraMapDef.h"
#include "Rules/CytraMapLoader.h"
#include "Rules/CytraWaveLoader.h"
#include "Rules/CytraWaveSchedule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CytraTest
{
	FString DataPath(const TCHAR* Relative)
	{
		return FPaths::ProjectContentDir() / TEXT("Data") / Relative;
	}

	bool LoadMapAndBalance(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance)
	{
		FString Error;
		if (!FCytraMapLoader::LoadFromFile(DataPath(TEXT("Maps/Passe01.json")), OutMap, Error)
			|| !FCytraBalanceLoader::LoadFromFile(DataPath(TEXT("Balance.json")), OutBalance, Error))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}

	bool LoadAll(FAutomationTestBase& Test, FCytraMapDef& OutMap, FCytraBalance& OutBalance, FCytraWaveSchedule& OutWaves)
	{
		if (!LoadMapAndBalance(Test, OutMap, OutBalance))
		{
			return false;
		}
		FString Error;
		if (!FCytraWaveLoader::LoadFromFile(DataPath(TEXT("Waves/Passe01.json")), OutBalance, OutMap, OutWaves, Error))
		{
			Test.AddError(Error);
			return false;
		}
		return true;
	}

	TSharedPtr<FJsonObject> ReadJsonObject(const TCHAR* Relative)
	{
		FString Json;
		TSharedPtr<FJsonObject> Object;
		if (FFileHelper::LoadFileToString(Json, *DataPath(Relative)))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
		}
		return Object;
	}

	FString WriteJson(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;
		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
		return Json;
	}
}

#endif
