// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraNest.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraShapes.h"

namespace
{
	// Egg layout of the web prototype, converted from its 40-pixel cells: golden-angle spiral, eggs 0.21 x 0.28 cells.
	constexpr double GoldenAngle = 2.39996;
	constexpr double SpiralStart = 5.0 / 40.0;
	constexpr double SpiralStep = 7.2 / 40.0;
	constexpr double EggWidth = 8.4 / 40.0;
	constexpr double EggLength = 11.2 / 40.0;
}

ACytraNest::ACytraNest()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Eggs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Eggs"));
	Eggs->SetupAttachment(RootComponent);
	Eggs->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	EggMesh = SphereFinder.Object;
	EggMaterial = MaterialFinder.Object;
	Eggs->SetStaticMesh(EggMesh);

	EggColor = CytraShapes::FromHex(TEXT("f3eedd"));
}

void ACytraNest::SetEggs(int32 Count, float CellSize)
{
	if (Count == ShownEggs)
	{
		return;
	}
	if (ShownEggs == INDEX_NONE && EggMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(EggMaterial, this);
		Material->SetVectorParameterValue(TEXT("Color"), EggColor);
		Eggs->SetMaterial(0, Material);
	}
	ShownEggs = Count;

	Eggs->ClearInstances();
	// Grid x runs along world +Y and grid y along world -X: an egg elongated along grid y is long in world X.
	const FVector Scale(EggLength * CellSize / CytraShapes::Size, EggWidth * CellSize / CytraShapes::Size, EggWidth * CellSize / CytraShapes::Size);
	for (int32 i = 0; i < Count; ++i)
	{
		const double Angle = i * GoldenAngle;
		const double Radius = SpiralStart + SpiralStep * FMath::Sqrt(static_cast<double>(i));
		const double GridX = FMath::Cos(Angle) * Radius;
		const double GridY = FMath::Sin(Angle) * Radius;
		const FVector Location(-GridY * CellSize, GridX * CellSize, EggWidth * CellSize * 0.5);
		Eggs->AddInstance(FTransform(FQuat::Identity, Location, Scale));
	}
}
