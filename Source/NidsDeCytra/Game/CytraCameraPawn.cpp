// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

ACytraCameraPawn::ACytraCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Camera->bUsePawnControlRotation = false;
}

void ACytraCameraPawn::FrameBounds(const FBox& Bounds)
{
	if (!Bounds.IsValid)
	{
		return;
	}

	float Aspect = 16.0f / 9.0f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			Aspect = static_cast<float>(Size.X) / Size.Y;
		}
	}

	// The horizontal field of view is fixed; the vertical one follows the aspect ratio.
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
	const float TanHalfV = TanHalfH / Aspect;
	const FVector Extent = Bounds.GetExtent() * FramingMargin;
	const float Height = FMath::Max(Extent.Y / TanHalfH, Extent.X / TanHalfV);

	const FVector Center = Bounds.GetCenter();
	SetActorLocationAndRotation(FVector(Center.X, Center.Y, Bounds.Max.Z + Height), FRotator::ZeroRotator);
}
