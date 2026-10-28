// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/CytraShapes.h"

/** Interface colors of the web prototype. */
struct FCytraPalette
{
	FLinearColor Rock950 = CytraShapes::FromHex(TEXT("10141b"));
	FLinearColor Rock900 = CytraShapes::FromHex(TEXT("171c25"));
	FLinearColor Rock800 = CytraShapes::FromHex(TEXT("1f2631"));
	FLinearColor Rock700 = CytraShapes::FromHex(TEXT("2a3340"));
	FLinearColor Rock600 = CytraShapes::FromHex(TEXT("3d4858"));
	FLinearColor Snow = CytraShapes::FromHex(TEXT("e9eef4"));
	FLinearColor Mist = CytraShapes::FromHex(TEXT("9ca8b8"));
	FLinearColor Ember = CytraShapes::FromHex(TEXT("f0a04b"));
	FLinearColor Gold = CytraShapes::FromHex(TEXT("f2cf63"));
	FLinearColor Silk = CytraShapes::FromHex(TEXT("bba8ea"));
	FLinearColor Blood = CytraShapes::FromHex(TEXT("e2695b"));
	FLinearColor Moss = CytraShapes::FromHex(TEXT("8cc48d"));
	/** Text of a warning message. */
	FLinearColor WarningMessage = CytraShapes::FromHex(TEXT("ffd9d3"));
	/** Refusals and warnings. */
	FLinearColor Warning = CytraShapes::FromHex(TEXT("ffb9ae"));

	/** Built on first use: sRGB conversion is not available during static initialization. */
	static const FCytraPalette& Get()
	{
		static const FCytraPalette Instance;
		return Instance;
	}
};
