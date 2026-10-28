// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraEnemy.h"

#include "Components/StaticMeshComponent.h"
#include "World/CytraLooks.h"
#include "World/CytraShapes.h"

namespace
{
	// Health bar of the web prototype, converted from its 40-pixel cells: at least 16 pixels long or 2.2 radii, 4 pixels wide,
	// its centre 8 pixels beyond the body.
	constexpr float BarMinLength = 16.0f / 40.0f;
	constexpr float BarLengthPerSize = 1.1f;
	constexpr float BarWidthCells = 4.0f / 40.0f;
	constexpr float BarGap = 8.0f / 40.0f;
	/** World units above the body top. */
	constexpr float BarLift = 10.0f;
	constexpr float BarThickness = 2.0f;

	/** The slow halo is a flat disc a little wider than the body. */
	constexpr float HaloScale = 1.35f;
	constexpr float HaloThickness = 2.0f;

	const TCHAR* const BarBackColor = TEXT("10141b");
	const TCHAR* const BarFillColor = TEXT("e2695b");
	const TCHAR* const SlowColor = TEXT("8fe9f2");
}

ACytraEnemy::ACytraEnemy()
{
	BarBack = CreatePart(TEXT("BarBack"), RootComponent);
	BarFill = CreatePart(TEXT("BarFill"), RootComponent);
	SlowHalo = CreatePart(TEXT("SlowHalo"), RootComponent);
	BarBack->SetVisibility(false);
	BarFill->SetVisibility(false);
	SlowHalo->SetVisibility(false);
}

void ACytraEnemy::SetLook(const FCytraLook& Look, float CellSize)
{
	Super::SetLook(Look, CellSize);

	const float Size = static_cast<float>(Look.Size) * CellSize;
	BarLength = FMath::Max(BarMinLength, BarLengthPerSize * static_cast<float>(Look.Size)) * CellSize;
	BarWidth = BarWidthCells * CellSize;
	// North of the enemy is world +X.
	const FVector BarCenter(Size * 0.5f + BarGap * CellSize, 0.0f, Size + BarLift);

	ApplyLook(*BarBack, FCytraLook{ TEXT("Cube"), CytraShapes::FromHex(BarBackColor), 1.0 });
	ApplyLook(*BarFill, FCytraLook{ TEXT("Cube"), CytraShapes::FromHex(BarFillColor), 1.0 });
	BarBack->SetRelativeLocation(BarCenter);
	BarBack->SetRelativeScale3D(FVector(BarWidth, BarLength, BarThickness) / CytraShapes::Size);
	BarFill->SetRelativeLocation(BarCenter + FVector(0.0f, 0.0f, BarThickness));

	ApplyLook(*SlowHalo, FCytraLook{ TEXT("Cylinder"), CytraShapes::FromHex(SlowColor), 1.0 });
	SlowHalo->SetRelativeLocation(FVector(0.0f, 0.0f, HaloThickness * 0.5f));
	SlowHalo->SetRelativeScale3D(FVector(Size * HaloScale, Size * HaloScale, HaloThickness) / CytraShapes::Size);
	ShownHealth = -1.0f;
}

void ACytraEnemy::ShowStatus(float Health, bool bSlowed)
{
	SlowHalo->SetVisibility(bSlowed);
	Health = FMath::Clamp(Health, 0.0f, 1.0f);
	if (Health == ShownHealth)
	{
		return;
	}
	ShownHealth = Health;
	const bool bWounded = Health < 1.0f;
	BarBack->SetVisibility(bWounded);
	BarFill->SetVisibility(bWounded && Health > 0.0f);
	if (bWounded)
	{
		// The fill shrinks toward the west end of the bar.
		const float Length = BarLength * Health;
		FVector Location = BarFill->GetRelativeLocation();
		Location.Y = (Length - BarLength) * 0.5f;
		BarFill->SetRelativeLocation(Location);
		BarFill->SetRelativeScale3D(FVector(BarWidth, FMath::Max(Length, 0.01f), BarThickness) / CytraShapes::Size);
	}
}
