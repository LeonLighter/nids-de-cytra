// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraJson.h"

#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace CytraJson
{
	bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& OutRoot, FString& OutError)
	{
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot) || !OutRoot.IsValid())
		{
			OutError = TEXT("invalid JSON");
			return false;
		}
		return true;
	}

	bool LoadFile(const FString& Path, FString& OutJson, FString& OutError)
	{
		if (!FFileHelper::LoadFileToString(OutJson, *Path))
		{
			OutError = FString::Printf(TEXT("cannot read %s"), *Path);
			return false;
		}
		return true;
	}

	FString FReader::FieldPath(const TCHAR* Field) const
	{
		if (!Field)
		{
			return Path;
		}
		return Path.IsEmpty() ? FString(Field) : FString::Printf(TEXT("%s.%s"), *Path, Field);
	}

	bool FReader::Fail(const TCHAR* Field, const FString& Message) const
	{
		Error = FString::Printf(TEXT("%s: %s"), *FieldPath(Field), *Message);
		return false;
	}

	bool FReader::Number(const TCHAR* Field, double& Out) const
	{
		if (!Obj.HasField(Field))
		{
			return Fail(Field, TEXT("missing number"));
		}
		return OptionalNumber(Field, Out);
	}

	bool FReader::OptionalNumber(const TCHAR* Field, double& Out) const
	{
		const TSharedPtr<FJsonValue> Value = Obj.TryGetField(Field);
		if (!Value.IsValid())
		{
			return true;
		}
		if (Value->Type != EJson::Number)
		{
			return Fail(Field, TEXT("must be a number"));
		}
		Out = Value->AsNumber();
		return true;
	}

	bool FReader::OptionalBool(const TCHAR* Field, bool& Out) const
	{
		const TSharedPtr<FJsonValue> Value = Obj.TryGetField(Field);
		if (!Value.IsValid())
		{
			return true;
		}
		if (Value->Type != EJson::Boolean)
		{
			return Fail(Field, TEXT("must be true or false"));
		}
		Out = Value->AsBool();
		return true;
	}

	bool FReader::Integer(const TCHAR* Field, int32& Out) const
	{
		if (!Obj.HasField(Field))
		{
			return Fail(Field, TEXT("missing integer"));
		}
		return OptionalInteger(Field, Out);
	}

	bool FReader::OptionalInteger(const TCHAR* Field, int32& Out) const
	{
		double Value = Out;
		if (!OptionalNumber(Field, Value))
		{
			return false;
		}
		if (Value != FMath::FloorToDouble(Value) || FMath::Abs(Value) > MAX_int32)
		{
			return Fail(Field, TEXT("must be an integer"));
		}
		Out = static_cast<int32>(Value);
		return true;
	}

	bool FReader::String(const TCHAR* Field, FString& Out) const
	{
		if (!Obj.TryGetStringField(Field, Out) || Out.IsEmpty())
		{
			return Fail(Field, TEXT("missing string"));
		}
		return true;
	}

	bool FReader::Name(const TCHAR* Field, FName& Out) const
	{
		FString Value;
		if (!String(Field, Value))
		{
			return false;
		}
		Out = FName(*Value);
		return true;
	}

	bool FReader::Object(const TCHAR* Field, TSharedPtr<FJsonObject>& Out) const
	{
		const TSharedPtr<FJsonObject>* Value = nullptr;
		if (!Obj.TryGetObjectField(Field, Value))
		{
			return Fail(Field, TEXT("missing object"));
		}
		Out = *Value;
		return true;
	}

	bool FReader::OptionalObject(const TCHAR* Field, TSharedPtr<FJsonObject>& Out) const
	{
		Out.Reset();
		if (!Obj.HasField(Field))
		{
			return true;
		}
		return Object(Field, Out);
	}

	bool FReader::Array(const TCHAR* Field, const TArray<TSharedPtr<FJsonValue>>*& Out) const
	{
		if (!Obj.TryGetArrayField(Field, Out))
		{
			return Fail(Field, TEXT("missing array"));
		}
		return true;
	}
}
