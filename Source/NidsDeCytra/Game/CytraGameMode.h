// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CytraGameMode.generated.h"

/** Runs the match. For now it only sets up the default classes. */
UCLASS()
class NIDSDECYTRA_API ACytraGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACytraGameMode();
};
