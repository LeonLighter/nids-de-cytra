// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rules/CytraOrders.h"

enum class ECytraWorkerState : uint8
{
	/** Waiting on a camp. */
	Idle,
	/** Walking to its job. */
	Move,
	/** On site, advancing its job. */
	Work,
	/** Walking to the nearest camp. */
	Back,
	/** Miner walking to the mine. */
	ToMine,
	/** Miner extracting gold. */
	Mining,
};

/** Free for a new job. Miners at the mine or on their way are only taken as a last resort. */
inline bool IsCytraWorkerFree(ECytraWorkerState State)
{
	return State == ECytraWorkerState::Idle || State == ECytraWorkerState::Back || State == ECytraWorkerState::ToMine || State == ECytraWorkerState::Mining;
}

/** A member of the crew. Positions are in grid cells. */
struct FCytraWorker
{
	int32 Id = 0;
	FName Role;
	FVector2d Position = FVector2d::ZeroVector;
	/** Cells still to walk, the next one first. */
	TArray<FIntPoint> Path;
	/** Id of the job taken, 0 if none. */
	int32 Job = 0;
	ECytraWorkerState State = ECytraWorkerState::Idle;
	/** Seconds spent mining since the last yield. */
	double MineTime = 0.0;
};

/** An order paid for and waiting to be carried out on a cell. */
struct FCytraJob
{
	int32 Id = 0;
	ECytraOrderKind Kind = ECytraOrderKind::Work;
	/** Work id or tower type. */
	FName Type;
	FIntPoint Cell = FIntPoint::ZeroValue;
	FName Role;
	int32 Cost = 0;
	/** Seconds of work on site. */
	double Time = 0.0;
	double Progress = 0.0;
	/** Id of the assigned worker, 0 while waiting for one. */
	int32 Worker = 0;
};

struct FCytraOrderResult
{
	FCytraOrderCheck Check;
	/** Id of the job created, 0 if the order was refused. */
	int32 Job = 0;
	/** Accepted but no worker is free yet. */
	bool bWaiting = false;

	bool IsAccepted() const { return Check.IsAccepted(); }
};

/** Why a recruitment is refused, in the order the checks run. */
enum class ECytraRecruitRefusal : uint8
{
	None,
	/** The specialty is not in the balance data. */
	UnknownRole,
	CrewFull,
	NotEnoughGold,
};

struct FCytraRecruitCheck
{
	ECytraRecruitRefusal Refusal = ECytraRecruitRefusal::None;
	int32 Cost = 0;
	int32 MissingGold = 0;
	/** Crew size limit, for the refusal message. */
	int32 MaxCrew = 0;
	/** Id of the recruit, once recruited. */
	int32 Worker = 0;

	bool IsAccepted() const { return Refusal == ECytraRecruitRefusal::None; }
};

/** Why a dismissal is refused, in the order the checks run. */
enum class ECytraDismissRefusal : uint8
{
	None,
	GameOver,
	UnknownWorker,
};

struct FCytraDismissCheck
{
	ECytraDismissRefusal Refusal = ECytraDismissRefusal::None;
	FName Role;
	/** Gold paid back. */
	int32 Refund = 0;
	/** Job the worker had taken, which waits for another worker, 0 if none. */
	int32 Job = 0;

	bool IsAccepted() const { return Refusal == ECytraDismissRefusal::None; }
};
