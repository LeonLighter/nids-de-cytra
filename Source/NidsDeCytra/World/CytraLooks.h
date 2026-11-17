// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraTileType.h"

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

/** The isometric camera: an orthographic view at a fixed angle, zoom and panning. */
struct FCytraCameraLook
{
	/** Tilt below the horizon and turn around the vertical, in degrees. True isometry seen from the south-west: -35.264 and 45. */
	double Pitch = -35.264;
	double Yaw = 45.0;
	/** Space around the whole board, as a factor of its size on screen. */
	double Margin = 1.05;
	/** Closest zoom, as a factor of the whole board view. */
	double MaxZoom = 3.0;
	/** Zoom factor of one mouse wheel notch. */
	double ZoomStep = 1.2;
	/** Keyboard panning speed, in screen pixels per second. */
	double PanSpeed = 900.0;
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
	/** Optional in the file: defaults to true isometry. */
	FCytraCameraLook Camera;
	/** Height of the top of each tile type, in world units. Optional: the board keeps its own height for a type left out. */
	TMap<ECytraTileType, float> TileHeights;

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
