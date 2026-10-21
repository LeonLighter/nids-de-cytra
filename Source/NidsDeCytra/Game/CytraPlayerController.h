// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CytraPlayerController.generated.h"

class UCytraHudWidget;

/** Mouse-driven controller: the cursor stays visible, the game is played by clicking tiles. */
UCLASS()
class NIDSDECYTRA_API ACytraPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACytraPlayerController();

	/** Order from the interface: launch the next wave now. */
	void LaunchWave();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UCytraHudWidget> Hud;
};
