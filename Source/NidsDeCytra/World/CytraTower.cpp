// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraTower.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "World/CytraShapes.h"

namespace
{
	/** Height of the base, as a share of its width. */
	constexpr float BaseHeightRatio = 0.4f;
	/** Width and height of an aiming arm, in cells. */
	constexpr float ArmWidth = 0.08f;
	/** The ballista bolt of the web prototype runs from 9 pixels behind the centre to 14 ahead: its middle is 2.5 pixels ahead. */
	constexpr float ArmForward = 2.5f / 23.0f;
	/** A flat top (a cylinder) is a disc this thick, in world units. */
	constexpr float DiscThickness = 4.0f;

	// Level pips of the web prototype: 4-pixel squares, 6 pixels apart, starting 8 pixels west of the centre, 14 pixels south of it.
	constexpr float PipSize = 4.0f / 40.0f;
	constexpr float PipSpacing = 6.0f / 40.0f;
	constexpr float PipStart = -8.0f / 40.0f;
	constexpr float PipSouth = 16.0f / 40.0f;
	constexpr float PipThickness = 3.0f;
	const TCHAR* const PipColor = TEXT("f2cf63");
}

ACytraTower::ACytraTower()
{
	Turret = CreateDefaultSubobject<USceneComponent>(TEXT("Turret"));
	Turret->SetupAttachment(RootComponent);
	Top = CreatePart(TEXT("Top"), Turret);

	Pips = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Pips"));
	Pips->SetupAttachment(RootComponent);
	Pips->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pips->SetGenerateOverlapEvents(false);
}

void ACytraTower::SetTowerLook(FName InType, const FCytraTowerLook& Look, float InCellSize)
{
	Type = InType;
	bAims = Look.bAims;
	CellSize = InCellSize;

	const float BaseWidth = static_cast<float>(Look.Base.Size) * CellSize;
	const float BaseHeight = BaseWidth * BaseHeightRatio;
	if (ApplyLook(*Body, Look.Base))
	{
		Body->SetRelativeScale3D(FVector(BaseWidth, BaseWidth, BaseHeight) / CytraShapes::Size);
		Body->SetRelativeLocation(FVector(0.0f, 0.0f, BaseHeight * 0.5f));
	}
	Turret->SetRelativeLocation(FVector(0.0f, 0.0f, BaseHeight));

	const float TopSize = static_cast<float>(Look.Top.Size) * CellSize;
	if (ApplyLook(*Top, Look.Top))
	{
		if (bAims)
		{
			// An arm along the turret's forward axis.
			const float Width = ArmWidth * CellSize;
			Top->SetRelativeScale3D(FVector(TopSize, Width, Width) / CytraShapes::Size);
			Top->SetRelativeLocation(FVector(TopSize * ArmForward, 0.0f, Width * 0.5f));
		}
		else if (Look.Top.Shape == TEXT("Cylinder"))
		{
			Top->SetRelativeScale3D(FVector(TopSize, TopSize, DiscThickness) / CytraShapes::Size);
			Top->SetRelativeLocation(FVector(0.0f, 0.0f, DiscThickness * 0.5f));
		}
		else
		{
			Top->SetRelativeScale3D(FVector(TopSize / CytraShapes::Size));
			Top->SetRelativeLocation(FVector(0.0f, 0.0f, TopSize * 0.5f));
		}
	}

	if (const TObjectPtr<UStaticMesh>* Cube = ShapeMeshes.Find(TEXT("Cube")))
	{
		Pips->SetStaticMesh(*Cube);
		Pips->SetMaterial(0, MakeColor(CytraShapes::FromHex(PipColor)));
	}
	ShownLevel = -1;
	ShownScale = -1.0f;
}

void ACytraTower::ShowState(int32 Level, float AimYaw, float Scale)
{
	if (bAims)
	{
		Turret->SetRelativeRotation(FRotator(0.0f, AimYaw, 0.0f));
	}
	if (Scale != ShownScale)
	{
		ShownScale = Scale;
		SetActorScale3D(FVector(Scale));
	}
	if (Level != ShownLevel)
	{
		ShownLevel = Level;
		Pips->ClearInstances();
		for (int32 Index = 0; Index < Level; ++Index)
		{
			// South of the slot centre is world -X, east is +Y.
			const FVector Location(-PipSouth * CellSize, (PipStart + PipSpacing * Index + PipSize * 0.5f) * CellSize, PipThickness * 0.5f);
			Pips->AddInstance(FTransform(FQuat::Identity, Location, FVector(PipSize * CellSize, PipSize * CellSize, PipThickness) / CytraShapes::Size));
		}
	}
}
