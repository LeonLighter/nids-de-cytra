// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/CytraShots.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/CytraBoard.h"
#include "World/CytraShapes.h"

namespace
{
	/** World height of the shots: above the towers on the high slots. */
	constexpr float ShotHeight = 110.0f;
	constexpr float Thin = 3.0f;

	// Sizes of the web prototype, in its 40-pixel cells.
	constexpr float BoltLength = 11.0f / 40.0f;
	constexpr float BoltWidth = 2.0f / 40.0f;
	/** A shell grows from 8 to 12 pixels wide and rises 26 pixels at the top of its arc. */
	constexpr float ShellSize = 8.0f / 40.0f;
	constexpr float ShellGrowth = 4.0f / 40.0f;
	constexpr float ShellRise = 26.0f / 40.0f;
	constexpr float ZapWidth = 2.0f / 40.0f;
	/** Each zap bends at 3 points, up to 6 pixels aside. */
	constexpr int32 ZapPieces = 4;
	constexpr float ZapJitter = 12.0f / 40.0f;
	constexpr float BlastWidth = 3.0f / 40.0f;
	constexpr int32 BlastSegments = 40;

	constexpr float ZapSeconds = 0.16f;
	constexpr float BlastSeconds = 0.35f;

	/** The jitter of the prototype: a fixed pseudo-random number in [0, 1). */
	double Hash(double X, double Y)
	{
		const double N = FMath::Sin(X * 127.1 + Y * 311.7) * 43758.5453;
		return N - FMath::FloorToDouble(N);
	}

	FVector At(const ACytraBoard& Board, const FVector2d& Position, float Height)
	{
		return Board.GridToWorld(Position) + FVector(0.0f, 0.0f, Height);
	}
}

ACytraShots::ACytraShots()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	SphereMesh = SphereFinder.Object;
	BaseMaterial = MaterialFinder.Object;
}

UInstancedStaticMeshComponent* ACytraShots::MakeInstances(FName Name, UStaticMesh* Mesh, const TCHAR* Hex)
{
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, Name);
	Component->SetupAttachment(RootComponent);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetStaticMesh(Mesh);
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
		Material->SetVectorParameterValue(TEXT("Color"), CytraShapes::FromHex(Hex));
		Component->SetMaterial(0, Material);
	}
	Component->RegisterComponent();
	return Component;
}

void ACytraShots::Show(const ACytraBoard& Board, const TArray<FCytraShot>& Shots, TArray<FCytraEffect> NewEffects, float DeltaSeconds)
{
	if (!Bolts)
	{
		Bolts = MakeInstances(TEXT("Bolts"), CubeMesh, TEXT("f5f0dc"));
		Shells = MakeInstances(TEXT("Shells"), SphereMesh, TEXT("14181f"));
		Zaps = MakeInstances(TEXT("Zaps"), CubeMesh, TEXT("8fe9f2"));
		Blasts = MakeInstances(TEXT("Blasts"), CubeMesh, TEXT("f0a04b"));
	}
	Bolts->ClearInstances();
	Shells->ClearInstances();
	Zaps->ClearInstances();
	Blasts->ClearInstances();

	const float Cell = ACytraBoard::CellSize;
	for (const FCytraShot& Shot : Shots)
	{
		if (Shot.Kind == ECytraShotKind::Bolt)
		{
			const FVector2d Direction(FMath::Cos(Shot.Angle), FMath::Sin(Shot.Angle));
			const FVector Tail = At(Board, Shot.Position - Direction * (BoltLength * 0.5), ShotHeight);
			const FVector Head = At(Board, Shot.Position + Direction * (BoltLength * 0.5), ShotHeight);
			CytraShapes::AddSegment(*Bolts, Tail, Head, BoltWidth * Cell, Thin);
		}
		else
		{
			const double Flight = Shot.Duration > 0.0 ? FMath::Min(1.0, Shot.Elapsed / Shot.Duration) : 1.0;
			const float Arc = FMath::Sin(static_cast<float>(Flight) * PI);
			const FVector Location = At(Board, FMath::Lerp(Shot.From, Shot.To, Flight), ShotHeight + Arc * ShellRise * Cell);
			Shells->AddInstance(FTransform(FQuat::Identity, Location, FVector((ShellSize + Arc * ShellGrowth) * Cell / CytraShapes::Size)), true);
		}
	}

	for (FCytraEffect& Effect : NewEffects)
	{
		Running.Add(FRunningEffect{ MoveTemp(Effect), 0.0f });
	}
	for (FRunningEffect& Entry : Running)
	{
		const FCytraEffect& Effect = Entry.Effect;
		if (Effect.Kind == ECytraEffectKind::Zap)
		{
			FVector2d Previous = Effect.From;
			for (int32 Piece = 1; Piece <= ZapPieces; ++Piece)
			{
				const double U = static_cast<double>(Piece) / ZapPieces;
				FVector2d Next = FMath::Lerp(Effect.From, Effect.To, U);
				if (Piece < ZapPieces)
				{
					Next += FVector2d(Hash(Piece, Effect.To.X * 9.0) - 0.5, Hash(Effect.To.Y * 7.0, Piece) - 0.5) * ZapJitter;
				}
				CytraShapes::AddSegment(*Zaps, At(Board, Previous, ShotHeight), At(Board, Next, ShotHeight), ZapWidth * Cell, Thin);
				Previous = Next;
			}
		}
		else
		{
			// The ring widens from 30 % of the splash radius to all of it.
			const float Ratio = FMath::Min(1.0f, Entry.Age / BlastSeconds);
			const float Radius = static_cast<float>(Effect.Radius) * (0.3f + 0.7f * Ratio) * Cell;
			CytraShapes::AddRing(*Blasts, At(Board, Effect.To, ShotHeight), Radius, BlastWidth * Cell, Thin, BlastSegments);
		}
		Entry.Age += DeltaSeconds;
	}
	Running.RemoveAll([](const FRunningEffect& Entry)
	{
		return Entry.Age >= (Entry.Effect.Kind == ECytraEffectKind::Zap ? ZapSeconds : BlastSeconds);
	});
}
