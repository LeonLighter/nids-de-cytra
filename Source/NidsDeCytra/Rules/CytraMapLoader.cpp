// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraMapLoader.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Rules/CytraMapDef.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString TileName(ECytraTileType Type)
	{
		return StaticEnum<ECytraTileType>()->GetNameStringByValue(static_cast<int64>(Type));
	}

	bool ParseLegend(const FJsonObject& Root, TMap<TCHAR, ECytraTileType>& OutLegend, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Legend = nullptr;
		if (!Root.TryGetObjectField(TEXT("legend"), Legend))
		{
			OutError = TEXT("missing 'legend' object");
			return false;
		}

		const UEnum* Enum = StaticEnum<ECytraTileType>();
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*Legend)->Values)
		{
			if (Entry.Key.Len() != 1)
			{
				OutError = FString::Printf(TEXT("legend key '%s' must be a single character"), *Entry.Key);
				return false;
			}
			const FString TypeName = Entry.Value->AsString();
			const int64 Value = Enum->GetValueByNameString(TypeName);
			if (Value == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("legend '%s' names unknown tile type '%s'"), *Entry.Key, *TypeName);
				return false;
			}
			OutLegend.Add(Entry.Key[0], static_cast<ECytraTileType>(Value));
		}
		return true;
	}

	bool ParseGrid(const FJsonObject& Root, const TMap<TCHAR, ECytraTileType>& Legend, FCytraMapDef& Map, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Grid = nullptr;
		if (!Root.TryGetArrayField(TEXT("grid"), Grid))
		{
			OutError = TEXT("missing 'grid' array");
			return false;
		}
		if (Grid->Num() != Map.Rows)
		{
			OutError = FString::Printf(TEXT("grid has %d rows, expected %d"), Grid->Num(), Map.Rows);
			return false;
		}

		Map.Tiles.Reset(Map.Cols * Map.Rows);
		for (int32 Y = 0; Y < Map.Rows; ++Y)
		{
			const FString Line = (*Grid)[Y]->AsString();
			if (Line.Len() != Map.Cols)
			{
				OutError = FString::Printf(TEXT("grid row %d has %d cells, expected %d"), Y, Line.Len(), Map.Cols);
				return false;
			}
			for (int32 X = 0; X < Map.Cols; ++X)
			{
				const ECytraTileType* Type = Legend.Find(Line[X]);
				if (!Type)
				{
					OutError = FString::Printf(TEXT("unknown symbol '%c' at (%d,%d)"), Line[X], X, Y);
					return false;
				}
				Map.Tiles.Add(*Type);

				const FIntPoint Cell(X, Y);
				if (*Type == ECytraTileType::Camp || *Type == ECytraTileType::Nest)
				{
					FIntPoint& Unique = *Type == ECytraTileType::Camp ? Map.Camp : Map.Nest;
					if (Unique != FIntPoint(INDEX_NONE))
					{
						OutError = FString::Printf(TEXT("second %s at (%d,%d)"), *TileName(*Type), X, Y);
						return false;
					}
					Unique = Cell;
				}
				else if (*Type == ECytraTileType::Mine)
				{
					Map.Mines.Add(Cell);
				}
				else if (*Type == ECytraTileType::OutpostSite)
				{
					Map.OutpostSites.Add(Cell);
				}
			}
		}

		if (Map.Camp == FIntPoint(INDEX_NONE) || Map.Nest == FIntPoint(INDEX_NONE))
		{
			OutError = TEXT("grid needs exactly one camp and one nest");
			return false;
		}
		return true;
	}

	bool ParsePoint(const TSharedPtr<FJsonValue>& Value, FIntPoint& OutPoint)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
		if (!Value->TryGetArray(Pair) || Pair->Num() != 2)
		{
			return false;
		}
		int32 X = 0;
		int32 Y = 0;
		if (!(*Pair)[0]->TryGetNumber(X) || !(*Pair)[1]->TryGetNumber(Y))
		{
			return false;
		}
		OutPoint = FIntPoint(X, Y);
		return true;
	}

	bool ValidateRoute(const FCytraMapDef& Map, const FCytraRoute& Route, FString& OutError)
	{
		const FString Id = Route.Id.ToString();
		if (Route.Waypoints.Num() < 2)
		{
			OutError = FString::Printf(TEXT("route %s needs at least two waypoints"), *Id);
			return false;
		}
		if (Route.Waypoints.Last() != Map.Nest)
		{
			OutError = FString::Printf(TEXT("route %s does not end on the nest"), *Id);
			return false;
		}

		for (int32 i = 0; i + 1 < Route.Waypoints.Num(); ++i)
		{
			const FIntPoint From = Route.Waypoints[i];
			const FIntPoint To = Route.Waypoints[i + 1];
			if ((From.X != To.X && From.Y != To.Y) || From == To)
			{
				OutError = FString::Printf(TEXT("route %s: segment (%d,%d)-(%d,%d) is not horizontal or vertical"),
					*Id, From.X, From.Y, To.X, To.Y);
				return false;
			}

			const FIntPoint Step(FMath::Sign(To.X - From.X), FMath::Sign(To.Y - From.Y));
			for (FIntPoint Cell = From;; Cell += Step)
			{
				const ECytraTileType Type = Map.Get(Cell);
				if (Map.IsInside(Cell) && Type != ECytraTileType::Road && Type != ECytraTileType::Nest)
				{
					OutError = FString::Printf(TEXT("route %s crosses %s at (%d,%d)"), *Id, *TileName(Type), Cell.X, Cell.Y);
					return false;
				}
				if (Cell == To)
				{
					break;
				}
			}
		}
		return true;
	}

	bool ParseRoutes(const FJsonObject& Root, FCytraMapDef& Map, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Routes = nullptr;
		if (!Root.TryGetArrayField(TEXT("routes"), Routes) || Routes->IsEmpty())
		{
			OutError = TEXT("missing or empty 'routes' array");
			return false;
		}

		for (const TSharedPtr<FJsonValue>& RouteValue : *Routes)
		{
			const TSharedPtr<FJsonObject>* RouteObject = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Waypoints = nullptr;
			FString Id;
			if (!RouteValue->TryGetObject(RouteObject)
				|| !(*RouteObject)->TryGetStringField(TEXT("id"), Id)
				|| !(*RouteObject)->TryGetArrayField(TEXT("waypoints"), Waypoints))
			{
				OutError = TEXT("each route needs an 'id' and a 'waypoints' array");
				return false;
			}

			FCytraRoute& Route = Map.Routes.AddDefaulted_GetRef();
			Route.Id = FName(*Id);
			for (const TSharedPtr<FJsonValue>& PointValue : *Waypoints)
			{
				if (!ParsePoint(PointValue, Route.Waypoints.AddDefaulted_GetRef()))
				{
					OutError = FString::Printf(TEXT("route %s: waypoints must be [x, y] pairs"), *Id);
					return false;
				}
			}
			if (!ValidateRoute(Map, Route, OutError))
			{
				return false;
			}
		}
		return true;
	}
}

namespace
{
	bool ParseLabels(const FJsonObject& Root, FCytraMapDef& Map, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Labels = nullptr;
		if (!Root.TryGetArrayField(TEXT("labels"), Labels))
		{
			return true;
		}
		for (const TSharedPtr<FJsonValue>& LabelValue : *Labels)
		{
			const TSharedPtr<FJsonObject>* LabelObject = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* At = nullptr;
			FString Id;
			double X = 0.0;
			double Y = 0.0;
			if (!LabelValue->TryGetObject(LabelObject)
				|| !(*LabelObject)->TryGetStringField(TEXT("id"), Id)
				|| !(*LabelObject)->TryGetArrayField(TEXT("at"), At)
				|| At->Num() != 2 || !(*At)[0]->TryGetNumber(X) || !(*At)[1]->TryGetNumber(Y))
			{
				OutError = TEXT("each label needs an 'id' and an 'at' pair [x, y]");
				return false;
			}
			FCytraMapLabel& Label = Map.Labels.AddDefaulted_GetRef();
			Label.Id = FName(*Id);
			Label.At = FVector2d(X, Y);
			FString Align = TEXT("left");
			(*LabelObject)->TryGetStringField(TEXT("align"), Align);
			if (Align == TEXT("left"))
			{
				Label.Align = ECytraLabelAlign::Left;
			}
			else if (Align == TEXT("center"))
			{
				Label.Align = ECytraLabelAlign::Center;
			}
			else if (Align == TEXT("right"))
			{
				Label.Align = ECytraLabelAlign::Right;
			}
			else
			{
				OutError = FString::Printf(TEXT("label %s: 'align' must be left, center or right"), *Id);
				return false;
			}
			(*LabelObject)->TryGetNumberField(TEXT("size"), Label.Size);
		}
		return true;
	}
}

bool FCytraMapLoader::ParseJson(const FString& Json, FCytraMapDef& OutMap, FString& OutError)
{
	OutMap = FCytraMapDef();

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = TEXT("invalid JSON");
		return false;
	}

	FString Id;
	if (!Root->TryGetStringField(TEXT("id"), Id)
		|| !Root->TryGetNumberField(TEXT("cols"), OutMap.Cols)
		|| !Root->TryGetNumberField(TEXT("rows"), OutMap.Rows)
		|| OutMap.Cols <= 0 || OutMap.Rows <= 0)
	{
		OutError = TEXT("'id', 'cols' and 'rows' are required, with positive sizes");
		return false;
	}
	OutMap.Id = FName(*Id);

	TMap<TCHAR, ECytraTileType> Legend;
	return ParseLegend(*Root, Legend, OutError)
		&& ParseGrid(*Root, Legend, OutMap, OutError)
		&& ParseRoutes(*Root, OutMap, OutError)
		&& ParseLabels(*Root, OutMap, OutError);
}

bool FCytraMapLoader::LoadFromFile(const FString& Path, FCytraMapDef& OutMap, FString& OutError)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = FString::Printf(TEXT("cannot read %s"), *Path);
		return false;
	}
	if (!ParseJson(Json, OutMap, OutError))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *Path, *OutError);
		return false;
	}
	return true;
}
