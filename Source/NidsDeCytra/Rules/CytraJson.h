// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Helpers for the data loaders: typed field access that reports the full path of the faulty field. */
namespace CytraJson
{
	bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& OutRoot, FString& OutError);
	bool LoadFile(const FString& Path, FString& OutJson, FString& OutError);

	/** Reads fields of one JSON object. The first failure is written to Error, prefixed with the field path. */
	class FReader
	{
	public:
		FReader(const FJsonObject& InObject, FString InPath, FString& InError)
			: Obj(InObject), Path(MoveTemp(InPath)), Error(InError)
		{
		}

		bool Number(const TCHAR* Field, double& Out) const;
		/** Keeps Out unchanged when the field is absent. */
		bool OptionalNumber(const TCHAR* Field, double& Out) const;
		bool Integer(const TCHAR* Field, int32& Out) const;
		bool OptionalInteger(const TCHAR* Field, int32& Out) const;
		/** Keeps Out unchanged when the field is absent. */
		bool OptionalBool(const TCHAR* Field, bool& Out) const;
		bool String(const TCHAR* Field, FString& Out) const;
		bool Name(const TCHAR* Field, FName& Out) const;
		bool Object(const TCHAR* Field, TSharedPtr<FJsonObject>& Out) const;
		/** Leaves Out null when the field is absent. */
		bool OptionalObject(const TCHAR* Field, TSharedPtr<FJsonObject>& Out) const;
		bool Array(const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>*& Out) const;

		/** Records an error about Field (or about this object when Field is null). Always returns false. */
		bool Fail(const TCHAR* Field, const FString& Message) const;

		FString FieldPath(const TCHAR* Field) const;

	private:
		const FJsonObject& Obj;
		FString Path;
		FString& Error;
	};
}
