// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraLooks.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Rules/CytraJson.h"

using CytraJson::FReader;

const TCHAR* const FCytraLooks::Shapes[4] = { TEXT("Sphere"), TEXT("Cube"), TEXT("Cylinder"), TEXT("Cone") };

const FCytraLook& FCytraLooks::EnemyLook(FName Enemy) const
{
	static const FCytraLook Default;
	const FCytraLook* Look = Enemies.Find(Enemy);
	return Look ? *Look : Default;
}

bool FCytraLooks::ParseJson(const FString& Json, FCytraLooks& OutLooks, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!CytraJson::ParseObject(Json, Root, OutError))
	{
		return false;
	}

	const FReader RootReader(*Root, FString(), OutError);
	TSharedPtr<FJsonObject> EnemiesObject;
	if (!RootReader.Object(TEXT("enemies"), EnemiesObject))
	{
		return false;
	}

	OutLooks.Enemies.Reset();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : EnemiesObject->Values)
	{
		const FString Path = FString(TEXT("enemies.")) + Entry.Key;
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Entry.Value->TryGetObject(Object))
		{
			OutError = Path + TEXT(": must be an object");
			return false;
		}

		const FReader Reader(**Object, Path, OutError);
		FCytraLook Look;
		FString ColorHex;
		if (!Reader.Name(TEXT("shape"), Look.Shape) || !Reader.String(TEXT("color"), ColorHex) || !Reader.Number(TEXT("size"), Look.Size))
		{
			return false;
		}
		const bool bKnownShape = Algo::AnyOf(Shapes, [&Look](const TCHAR* Shape) { return Look.Shape == Shape; });
		if (!bKnownShape)
		{
			return Reader.Fail(TEXT("shape"), FString::Printf(TEXT("unknown shape %s (Sphere, Cube, Cylinder or Cone)"), *Look.Shape.ToString()));
		}
		if (Look.Size <= 0.0)
		{
			return Reader.Fail(TEXT("size"), TEXT("must be positive"));
		}
		const FString Digits = ColorHex.StartsWith(TEXT("#")) ? ColorHex.RightChop(1) : ColorHex;
		const bool bValidColor = Digits.Len() == 6 && Algo::AllOf(Digits, [](TCHAR C) { return FChar::IsHexDigit(C); });
		if (!bValidColor)
		{
			return Reader.Fail(TEXT("color"), FString::Printf(TEXT("expected 6 hexadecimal digits, got %s"), *ColorHex));
		}
		Look.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Digits));
		OutLooks.Enemies.Add(FName(*Entry.Key), Look);
	}
	return true;
}

bool FCytraLooks::LoadFromFile(const FString& Path, FCytraLooks& OutLooks, FString& OutError)
{
	FString Json;
	if (!CytraJson::LoadFile(Path, Json, OutError))
	{
		return false;
	}
	return ParseJson(Json, OutLooks, OutError);
}
