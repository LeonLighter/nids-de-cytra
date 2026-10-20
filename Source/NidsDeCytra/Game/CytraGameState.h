// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CytraGameState.generated.h"

/** Replicated view of the match (gold, eggs, wave, phase). Filled in by later milestones. */
UCLASS()
class NIDSDECYTRA_API ACytraGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ACytraGameState();
};
