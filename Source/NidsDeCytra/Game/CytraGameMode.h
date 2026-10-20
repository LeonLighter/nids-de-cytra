// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CytraGameMode.generated.h"

class ACytraBoard;

/** Runs the match. For now it sets up the board, the camera and fallback lighting. */
UCLASS()
class NIDSDECYTRA_API ACytraGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACytraGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** The board placed in the level, or a new one spawned at the origin. Built on first access. */
	ACytraBoard* GetOrCreateBoard();

private:
	/** The level ships without lights: add a sun, an atmosphere and a sky light if it has none. */
	void SpawnFallbackLighting();

	UPROPERTY(Transient)
	TObjectPtr<ACytraBoard> Board;
};
