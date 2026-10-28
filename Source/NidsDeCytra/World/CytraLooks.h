// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Placeholder appearance: an engine basic shape and a flat color. Sizes are in cells. */
struct FCytraLook
{
	/** Sphere, Cube, Cylinder or Cone, from /Engine/BasicShapes. */
	FName Shape = TEXT("Sphere");
	FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f);
	double Size = 0.35;
};

/** A tower: a squat base on the slot and a top piece on it. */
struct FCytraTowerLook
{
	FCytraLook Base = FCytraLook{ TEXT("Cylinder"), FLinearColor(0.5f, 0.5f, 0.5f), 0.55 };
	FCytraLook Top = FCytraLook{ TEXT("Sphere"), FLinearColor(0.8f, 0.8f, 0.8f), 0.25 };
	/** The top is an arm of length Top.Size that turns toward the target, like the bow of a ballista. */
	bool bAims = false;
};

/** Appearance of game objects, read from Content/Data/Looks.json. Display only: the rules never read it. */
struct NIDSDECYTRA_API FCytraLooks
{
	static const TCHAR* const Shapes[4];

	TMap<FName, FCytraLook> Enemies;
	/** By worker specialty. */
	TMap<FName, FCytraLook> Workers;
	/** By tower type. */
	TMap<FName, FCytraTowerLook> Towers;

	/** The look of a tower type, or the default look if the file has none. */
	const FCytraTowerLook& TowerLook(FName Tower) const;

	/** The look of an enemy type, or the default look if the file has none. */
	const FCytraLook& EnemyLook(FName Enemy) const;

	/** The look of a worker specialty, or the default look if the file has none. */
	const FCytraLook& WorkerLook(FName Role) const;

	/** On failure, OutError says why and OutLooks is left unspecified. */
	static bool ParseJson(const FString& Json, FCytraLooks& OutLooks, FString& OutError);
	static bool LoadFromFile(const FString& Path, FCytraLooks& OutLooks, FString& OutError);
};
