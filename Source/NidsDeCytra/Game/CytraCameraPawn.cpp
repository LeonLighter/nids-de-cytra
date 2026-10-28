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

	// The pawn carries the view angle; the camera looks along it.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->bUsePawnControlRotation = false;
	Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	// Fixed clip planes, set from the camera distance: the whole board always lies between them.
	Camera->SetAutoCalculateOrthoPlanes(false);
	Camera->SetUpdateOrthoPlanes(false);
	Camera->SetOrthoNearClipPlane(0.0f);
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
	if (bFramed && GEngine && GEngine->GameViewport && Viewport == GEngine->GameViewport->Viewport)
	{
		View.Clamp(GetScreen());
		ApplyView();
	}
}

void ACytraCameraPawn::SetCameraLook(const FCytraCameraLook& InLook)
{
	Look = InLook;
}

void ACytraCameraPawn::FrameBounds(const FBox& Bounds)
{
	if (!Bounds.IsValid)
	{
		return;
	}
	View = FCytraIsoView(Look, Bounds);
	bFramed = true;
	ApplyView();
}

void ACytraCameraPawn::Pan(const FVector2D& Direction, float DeltaSeconds)
{
	if (bFramed && !Direction.IsNearlyZero())
	{
		const FVector2d Clamped(FMath::Clamp(Direction.X, -1.0, 1.0), FMath::Clamp(Direction.Y, -1.0, 1.0));
		View.Pan(Clamped * Look.PanSpeed * DeltaSeconds, GetScreen());
		ApplyView();
	}
}

void ACytraCameraPawn::ZoomAt(float Notches, const FVector2D& CursorPixels)
{
	if (bFramed && !FMath::IsNearlyZero(Notches))
	{
		View.ZoomAt(FMath::Pow(Look.ZoomStep, static_cast<double>(Notches)), FVector2d(CursorPixels), GetScreen());
		ApplyView();
	}
}

FCytraScreen ACytraCameraPawn::GetScreen() const
{
	FCytraScreen Screen;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
		if (Size.X > 0 && Size.Y > 0)
		{
			// The interface is laid out in Slate units, scaled by the DPI curve.
			const float DpiScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size);
			Screen.Size = FVector2d(Size.X, Size.Y);
			Screen.Panel = FMath::Min(CytraLayout::PanelWidth * DpiScale, Size.X * 0.5);
			Screen.Bar = FMath::Min(CytraLayout::TopBarHeight * DpiScale, Size.Y * 0.5);
			return Screen;
		}
	}
	// No viewport yet: assume 16:9 with the interface at its nominal size.
	Screen.Panel = CytraLayout::PanelWidth;
	Screen.Bar = CytraLayout::TopBarHeight;
	return Screen;
}

void ACytraCameraPawn::ApplyView()
{
	const FCytraScreen Screen = GetScreen();
	SetActorLocationAndRotation(View.CameraLocation(Screen), View.GetRotation());
	Camera->SetOrthoWidth(static_cast<float>(View.OrthoWidth(Screen)));
	Camera->SetOrthoFarClipPlane(static_cast<float>(View.CameraDistance() * 2.0));
}
