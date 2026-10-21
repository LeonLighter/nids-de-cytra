// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraEnemy.h"

#include "Components/StaticMeshComponent.h"
#include "CytraLog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraLooks.h"
#include "World/CytraShapes.h"

ACytraEnemy::ACytraEnemy()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);

	for (const TCHAR* Shape : FCytraLooks::Shapes)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
		ShapeMeshes.Add(Shape, Finder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;
}

void ACytraEnemy::SetLook(const FCytraLook& Look, float CellSize)
{
	const TObjectPtr<UStaticMesh>* Mesh = ShapeMeshes.Find(Look.Shape);
	if (!Mesh || !*Mesh || !BaseMaterial)
	{
		UE_LOG(LogCytra, Warning, TEXT("Enemy: shape %s or base material not found"), *Look.Shape.ToString());
		return;
	}
	Body->SetStaticMesh(*Mesh);

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Look.Color);
	Body->SetMaterial(0, Material);

	const float Size = static_cast<float>(Look.Size) * CellSize;
	Body->SetRelativeScale3D(FVector(Size / CytraShapes::Size));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, Size * 0.5f));
}
