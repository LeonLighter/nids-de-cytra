// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraMapDef.h"

int32 FCytraMapDef::Count(ECytraTileType Type) const
{
	int32 N = 0;
	for (const ECytraTileType Tile : Tiles)
	{
		N += Tile == Type ? 1 : 0;
	}
	return N;
}
