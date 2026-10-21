// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Rules/CytraMatchPhase.h"
#include "CytraGameState.generated.h"

class FCytraMatch;

DECLARE_MULTICAST_DELEGATE(FOnCytraMatchChanged);

/** What the interface shows of the match, in display units. The game mode refreshes it after each frame of simulation. */
UCLASS()
class NIDSDECYTRA_API ACytraGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ACytraGameState();

	/** Copies the displayed values and broadcasts OnMatchChanged if any changed. */
	void UpdateFromMatch(const FCytraMatch& Match);

	/** Fires when a displayed value changes. */
	FOnCytraMatchChanged OnMatchChanged;

	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 Gold = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 Eggs = 0;

	/** Waves launched so far: during a wave, its 1-based number. */
	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 Wave = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 WaveCount = 0;

	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	ECytraMatchPhase Phase = ECytraMatchPhase::Prep;

	/** Whether the next wave launches on its own at the end of a countdown (false before wave 1). */
	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	bool bHasCountdown = false;

	/** Seconds left before the next wave, rounded up as displayed. */
	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 CountdownSeconds = 0;

	/** Gold earned by launching the next wave now. */
	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 EarlyLaunchBonus = 0;

	/** Enemies of the current wave still alive or still to spawn. */
	UPROPERTY(VisibleInstanceOnly, Category = "Cytra")
	int32 EnemiesLeft = 0;
};
