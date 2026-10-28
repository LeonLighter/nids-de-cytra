// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraLooks.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Rules/CytraJson.h"

using CytraJson::FReader;

const TCHAR* const FCytraLooks::Shapes[4] = { TEXT("Sphere"), TEXT("Cube"), TEXT("Cylinder"), TEXT("Cone") };

namespace
{
	const FCytraLook& FindOrDefault(const TMap<FName, FCytraLook>& Looks, FName Key)
	{
		static const FCytraLook Default;
		const FCytraLook* Look = Looks.Find(Key);
		return Look ? *Look : Default;
	}

	/** Reads a shape, a color and a size. */
	bool ParseLook(const FReader& Reader, FCytraLook& OutLook)
	{
		FString ColorHex;
		if (!Reader.Name(TEXT("shape"), OutLook.Shape) || !Reader.String(TEXT("color"), ColorHex) || !Reader.Number(TEXT("size"), OutLook.Size))
		{
			return false;
		}
		const bool bKnownShape = Algo::AnyOf(FCytraLooks::Shapes, [&OutLook](const TCHAR* Shape) { return OutLook.Shape == Shape; });
		if (!bKnownShape)
		{
			return Reader.Fail(TEXT("shape"), FString::Printf(TEXT("unknown shape %s (Sphere, Cube, Cylinder or Cone)"), *OutLook.Shape.ToString()));
		}
		if (OutLook.Size <= 0.0)
		{
			return Reader.Fail(TEXT("size"), TEXT("must be positive"));
		}
		const FString Digits = ColorHex.StartsWith(TEXT("#")) ? ColorHex.RightChop(1) : ColorHex;
		const bool bValidColor = Digits.Len() == 6 && Algo::AllOf(Digits, [](TCHAR C) { return FChar::IsHexDigit(C); });
		if (!bValidColor)
		{
			return Reader.Fail(TEXT("color"), FString::Printf(TEXT("expected 6 hexadecimal digits, got %s"), *ColorHex));
		}
		OutLook.Color = FLinearColor::FromSRGBColor(FColor::FromHex(Digits));
		return true;
	}

	/** Reads every look of one section of the file, such as "enemies". */
	bool ParseSection(const FReader& RootReader, const TCHAR* Section, TMap<FName, FCytraLook>& OutLooks, FString& OutError)
	{
		TSharedPtr<FJsonObject> SectionObject;
		if (!RootReader.Object(Section, SectionObject))
		{
			return false;
		}

		OutLooks.Reset();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : SectionObject->Values)
		{
			const FString Path = FString(Section) + TEXT(".") + Entry.Key;
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Entry.Value->TryGetObject(Object))
			{
				OutError = Path + TEXT(": must be an object");
				return false;
			}

			FCytraLook Look;
			if (!ParseLook(FReader(**Object, Path, OutError), Look))
			{
				return false;
			}
			OutLooks.Add(FName(*Entry.Key), Look);
		}
		return true;
	}

	/** Reads the "towers" section: a base and a top for each tower type. */
	bool ParseTowers(const FReader& RootReader, TMap<FName, FCytraTowerLook>& OutLooks, FString& OutError)
	{
		TSharedPtr<FJsonObject> SectionObject;
		if (!RootReader.Object(TEXT("towers"), SectionObject))
		{
			return false;
		}

		OutLooks.Reset();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : SectionObject->Values)
		{
			const FString Path = TEXT("towers.") + Entry.Key;
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Entry.Value->TryGetObject(Object))
			{
				OutError = Path + TEXT(": must be an object");
				return false;
			}

			const FReader Reader(**Object, Path, OutError);
			FCytraTowerLook Look;
			TSharedPtr<FJsonObject> Base;
			TSharedPtr<FJsonObject> Top;
			if (!Reader.Object(TEXT("base"), Base) || !ParseLook(FReader(*Base, Reader.FieldPath(TEXT("base")), OutError), Look.Base)
				|| !Reader.Object(TEXT("top"), Top) || !ParseLook(FReader(*Top, Reader.FieldPath(TEXT("top")), OutError), Look.Top)
				|| !Reader.OptionalBool(TEXT("aims"), Look.bAims))
			{
				return false;
			}
			OutLooks.Add(FName(*Entry.Key), Look);
		}
		return true;
	}
}

const FCytraTowerLook& FCytraLooks::TowerLook(FName Tower) const
{
	static const FCytraTowerLook Default;
	const FCytraTowerLook* Look = Towers.Find(Tower);
	return Look ? *Look : Default;
}

const FCytraLook& FCytraLooks::EnemyLook(FName Enemy) const
{
	return FindOrDefault(Enemies, Enemy);
}

const FCytraLook& FCytraLooks::WorkerLook(FName Role) const
{
	return FindOrDefault(Workers, Role);
}

bool FCytraLooks::ParseJson(const FString& Json, FCytraLooks& OutLooks, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!CytraJson::ParseObject(Json, Root, OutError))
	{
		return false;
	}
	const FReader RootReader(*Root, FString(), OutError);
	return ParseSection(RootReader, TEXT("enemies"), OutLooks.Enemies, OutError)
		&& ParseSection(RootReader, TEXT("workers"), OutLooks.Workers, OutError)
		&& ParseTowers(RootReader, OutLooks.Towers, OutError);
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
