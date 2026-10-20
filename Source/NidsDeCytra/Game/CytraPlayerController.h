// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CytraPlayerController.generated.h"

/** Mouse-driven controller: the cursor stays visible, the game is played by clicking tiles. */
UCLASS()
class NIDSDECYTRA_API ACytraPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACytraPlayerController();

protected:
	virtual void BeginPlay() override;
};
