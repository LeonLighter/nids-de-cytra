// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraCrew.h"

struct FCytraLaunchResult;
struct FCytraMatchEvent;

enum class ECytraMessageTone : uint8
{
	Good,
	/** An egg lost or an order refused. */
	Warning,
};

/** A brief message shown over the board, after toast() in the web prototype. */
struct FCytraMessage
{
	FText Text;
	ECytraMessageTone Tone = ECytraMessageTone::Good;
};

/** Player-facing messages for match events and order results, worded as in the prototype. Testable without any widget. */
struct NIDSDECYTRA_API FCytraMessages
{
	/** A job done, an egg lost or a wave cleared. Unset for events without a message, such as gold from the mine. */
	static TOptional<FCytraMessage> FromEvent(const FCytraMatchEvent& Event);

	/** The order accepted, with what it cost and whether it waits for a worker, or why it is refused. */
	static FCytraMessage FromOrder(const FCytraOrderResult& Result);

	/** The recruit on the way, or why the recruitment is refused. */
	static FCytraMessage FromRecruit(FName Role, const FCytraRecruitCheck& Check);

	/** The worker gone and the gold paid back, or why the dismissal is refused. */
	static FCytraMessage FromDismiss(const FCytraDismissCheck& Check);

	/** The early launch bonus. Unset when there is none. */
	static TOptional<FCytraMessage> FromLaunch(const FCytraLaunchResult& Result);
};
