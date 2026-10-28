// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraPiece.h"

#include "Components/StaticMeshComponent.h"
#include "CytraLog.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraLooks.h"
#include "World/CytraShapes.h"

ACytraPiece::ACytraPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Body = CreatePart(TEXT("Body"), RootComponent);

	for (const TCHAR* Shape : FCytraLooks::Shapes)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
		ShapeMeshes.Add(Shape, Finder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = MaterialFinder.Object;
}

UStaticMeshComponent* ACytraPiece::CreatePart(FName Name, USceneComponent* Parent)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	return Part;
}

UMaterialInterface* ACytraPiece::MakeColor(const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	return Material;
}

bool ACytraPiece::ApplyLook(UStaticMeshComponent& Component, const FCytraLook& Look)
{
	const TObjectPtr<UStaticMesh>* Mesh = ShapeMeshes.Find(Look.Shape);
	if (!Mesh || !*Mesh || !BaseMaterial)
	{
		UE_LOG(LogCytra, Warning, TEXT("Piece: shape %s or base material not found"), *Look.Shape.ToString());
		return false;
	}
	Component.SetStaticMesh(*Mesh);
	Component.SetMaterial(0, MakeColor(Look.Color));
	return true;
}

void ACytraPiece::SetLook(const FCytraLook& Look, float CellSize)
{
	if (!ApplyLook(*Body, Look))
	{
		return;
	}
	const float Size = static_cast<float>(Look.Size) * CellSize;
	Body->SetRelativeScale3D(FVector(Size / CytraShapes::Size));
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, Size * 0.5f));
}
