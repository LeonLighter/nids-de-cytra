// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/CytraCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "UI/CytraLayout.h"
#include "UnrealClient.h"

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

void ACytraCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	ResizedHandle = FViewport::ViewportResizedEvent.AddUObject(this, &ACytraCameraPawn::HandleViewportResized);
}

void ACytraCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FViewport::ViewportResizedEvent.Remove(ResizedHandle);
	Super::EndPlay(EndPlayReason);
}

void ACytraCameraPawn::HandleViewportResized(FViewport* Viewport, uint32 Unused)
{
	if (GEngine && GEngine->GameViewport && Viewport == GEngine->GameViewport->Viewport)
	{
		FrameBounds(FramedBounds);
	}
}

void ACytraCameraPawn::FrameBounds(const FBox& Bounds)
{
	if (!Bounds.IsValid)
	{
		return;
	}
	FramedBounds = Bounds;

	float Aspect = 16.0f / 9.0f;
	// Shares of the screen width and height taken by the order panel and the top bar.
	float PanelShare = 0.0f;
	float BarShare = 0.0f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			Aspect = static_cast<float>(Size.X) / Size.Y;
			const float DpiScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
			PanelShare = FMath::Clamp(CytraLayout::PanelWidth * DpiScale / Size.X, 0.0f, 0.5f);
			BarShare = FMath::Clamp(CytraLayout::TopBarHeight * DpiScale / Size.Y, 0.0f, 0.5f);
		}
	}

	// The horizontal field of view is fixed; the vertical one follows the aspect ratio.
	const float TanHalfH = FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView * 0.5f));
	const float TanHalfV = TanHalfH / Aspect;
	const FVector Extent = Bounds.GetExtent() * FramingMargin;
	const float Height = FMath::Max(Extent.Y / (TanHalfH * (1.0f - PanelShare)), Extent.X / (TanHalfV * (1.0f - BarShare)));

	// Shift the camera so the board sits in the middle of the free area: screen right is world +Y, screen up is world +X.
	const FVector Center = Bounds.GetCenter();
	const float OffsetY = PanelShare * Height * TanHalfH;
	const float OffsetX = BarShare * Height * TanHalfV;
	SetActorLocationAndRotation(FVector(Center.X + OffsetX, Center.Y + OffsetY, Bounds.Max.Z + Height), FRotator::ZeroRotator);
}
