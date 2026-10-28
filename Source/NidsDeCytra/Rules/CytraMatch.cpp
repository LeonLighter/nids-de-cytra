// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraMatch.h"

namespace
{
	/** Math.round of the prototype: halves round up. */
	FIntPoint RoundToCell(const FVector2d& Position)
	{
		return FIntPoint(FMath::FloorToInt32(Position.X + 0.5), FMath::FloorToInt32(Position.Y + 0.5));
	}

	bool IsNextTo(const FIntPoint& Cell, const FIntPoint& Target)
	{
		return FMath::Abs(Cell.X - Target.X) + FMath::Abs(Cell.Y - Target.Y) == 1;
	}

	/** The state a walk ends in. */
	ECytraWorkerState Arrival(ECytraWorkerState Walking)
	{
		switch (Walking)
		{
		case ECytraWorkerState::Move: return ECytraWorkerState::Work;
		case ECytraWorkerState::ToMine: return ECytraWorkerState::Mining;
		default: return ECytraWorkerState::Idle;
		}
	}

	bool IsWalking(ECytraWorkerState State)
	{
		return State == ECytraWorkerState::Move || State == ECytraWorkerState::Back || State == ECytraWorkerState::ToMine;
	}

	/** Cost added to a miner at the mine, so that any other free miner is chosen first. */
	constexpr int32 MineRecallPenalty = 1000;
}

FCytraMatch::FCytraMatch(const FCytraMapDef& InMap, const FCytraBalance& InBalance, const FCytraWaveSchedule& InWaves)
	: Map(InMap)
	, Balance(InBalance)
	, Waves(InWaves)
	, Grid(InMap)
	, Wallet(InBalance.StartGold)
	, Eggs(InBalance.StartEggs)
{
	for (const FCytraRoute& Route : Map.Routes)
	{
		Routes.Emplace(Route);
	}
	for (const TPair<FName, FCytraWorkDef>& Work : Balance.Works)
	{
		if (Work.Value.On == ECytraTileType::Mine)
		{
			MineRole = Work.Value.Role;
		}
	}
	for (const FName Role : Balance.StartRoles)
	{
		AddWorker(Role);
	}
}

void FCytraMatch::Step(double Dt)
{
	if (IsOver())
	{
		return;
	}
	Time += Dt;

	if (Phase == ECytraMatchPhase::Prep && Countdown.IsSet())
	{
		Countdown = Countdown.GetValue() - Dt;
		if (Countdown.GetValue() <= 0.0)
		{
			StartWave(true);
		}
	}
	if (Phase == ECytraMatchPhase::Wave)
	{
		WaveTime += Dt;
		int32 Due = 0;
		while (Due < SpawnQueue.Num() && SpawnQueue[Due].Time <= WaveTime)
		{
			Spawn(SpawnQueue[Due++]);
		}
		SpawnQueue.RemoveAt(0, Due);
	}

	MoveEnemies(Dt);
	// Towers and their shots come here (J40).
	Enemies.RemoveAll([](const FCytraEnemy& Enemy) { return Enemy.bDead; });
	StepWorkers(Dt);
	Manage();

	if (Eggs <= 0)
	{
		Phase = ECytraMatchPhase::Lost;
		return;
	}
	if (Phase == ECytraMatchPhase::Wave && SpawnQueue.IsEmpty() && Enemies.IsEmpty())
	{
		const int32 Bonus = Balance.WaveEndBonus(Wave);
		Wallet.Earn(Bonus);
		if (Wave >= Waves.Num())
		{
			Phase = ECytraMatchPhase::Won;
		}
		else
		{
			Phase = ECytraMatchPhase::Prep;
			// Wave is 1-based, so it is also the index of the next wave.
			Countdown = static_cast<double>(Waves.PrepTime(Wave, Balance));
			FCytraMatchEvent& Event = Events.AddDefaulted_GetRef();
			Event.Type = ECytraMatchEventType::WaveCleared;
			Event.Wave = Wave;
			Event.Gold = Bonus;
		}
	}
}

FCytraLaunchResult FCytraMatch::LaunchWave()
{
	return StartWave(false);
}

FCytraLaunchResult FCytraMatch::StartWave(bool bAuto)
{
	FCytraLaunchResult Result;
	if (Phase != ECytraMatchPhase::Prep || Wave >= Waves.Num())
	{
		return Result;
	}
	if (!bAuto && Countdown.IsSet())
	{
		Result.Bonus = Balance.EarlyLaunchBonus(Countdown.GetValue());
		Wallet.Earn(Result.Bonus);
	}
	SpawnQueue = Waves.SpawnQueue(Wave);
	++Wave;
	WaveTime = 0.0;
	Phase = ECytraMatchPhase::Wave;
	Countdown.Reset();
	Result.bLaunched = true;
	return Result;
}

void FCytraMatch::Spawn(const FCytraSpawn& Entry)
{
	const FCytraEnemyDef& Def = Balance.Enemies.FindChecked(Entry.Enemy);
	const int32 RouteIndex = Routes.IndexOfByPredicate([&Entry](const FCytraRoutePath& Path) { return Path.GetId() == Entry.Route; });
	check(RouteIndex != INDEX_NONE);

	FCytraEnemy& Enemy = Enemies.AddDefaulted_GetRef();
	Enemy.Id = ++NextId;
	Enemy.Type = Entry.Enemy;
	Enemy.Route = RouteIndex;
	Enemy.Position = Routes[RouteIndex].PositionAt(0.0);
	Enemy.MaxHp = Balance.EnemyHp(Entry.Enemy, Wave);
	Enemy.Hp = Enemy.MaxHp;
	Enemy.Speed = Def.Speed;
	Enemy.Gold = Def.Gold;
	Enemy.Eggs = Def.Eggs;
}

void FCytraMatch::MoveEnemies(double Dt)
{
	for (FCytraEnemy& Enemy : Enemies)
	{
		if (Enemy.bDead)
		{
			continue;
		}
		const FCytraRoutePath& Path = Routes[Enemy.Route];
		Enemy.Distance += Enemy.Speed * Dt;
		if (Enemy.Distance >= Path.GetLength())
		{
			// Reaching the nest destroys eggs but pays nothing.
			Enemy.bDead = true;
			Eggs = FMath::Max(0, Eggs - Enemy.Eggs);
			FCytraMatchEvent& Event = Events.AddDefaulted_GetRef();
			Event.Type = ECytraMatchEventType::Leak;
			Event.Enemy = Enemy.Type;
			Event.Eggs = Enemy.Eggs;
		}
		else
		{
			Enemy.Position = Path.PositionAt(Enemy.Distance);
		}
	}
}

void FCytraMatch::DamageEnemy(int32 EnemyId, double Damage)
{
	FCytraEnemy* Enemy = Enemies.FindByPredicate([EnemyId](const FCytraEnemy& E) { return E.Id == EnemyId; });
	if (!Enemy || Enemy->bDead)
	{
		return;
	}
	Enemy->Hp -= Damage;
	if (Enemy->Hp <= 0.0)
	{
		Enemy->bDead = true;
		Wallet.Earn(Enemy->Gold);
		++Kills;
	}
}

FCytraOrderCheck FCytraMatch::CheckOrder(const FIntPoint& Cell, const FCytraOrderAction& Action) const
{
	FCytraOrderContext Context;
	Context.bGameOver = IsOver();
	Context.bJobOnCell = FindJobAt(Cell) != nullptr;
	for (const FCytraWorker& Worker : Workers)
	{
		Context.CrewRoles.AddUnique(Worker.Role);
	}
	Context.Gold = Wallet.Gold;
	return FCytraOrders::Check(Grid, Balance, Cell, Action, Context);
}

FCytraOrderResult FCytraMatch::Order(const FIntPoint& Cell, const FCytraOrderAction& Action)
{
	FCytraOrderResult Result;
	Result.Check = CheckOrder(Cell, Action);
	if (!Result.IsAccepted())
	{
		return Result;
	}
	const FCytraOrderSpec& Spec = Result.Check.Spec.GetValue();
	Wallet.TrySpend(Spec.Cost);

	FCytraJob& Job = Jobs.AddDefaulted_GetRef();
	Job.Id = ++NextId;
	Job.Kind = Spec.Kind;
	Job.Type = Spec.Id;
	Job.Cell = Cell;
	Job.Role = Spec.Role;
	Job.Cost = Spec.Cost;
	Job.Time = Spec.Time;
	Result.Job = Job.Id;

	Manage();
	Result.bWaiting = FindJob(Result.Job)->Worker == 0;
	return Result;
}

const FCytraJob* FCytraMatch::FindJobAt(const FIntPoint& Cell) const
{
	return Jobs.FindByPredicate([&Cell](const FCytraJob& Job) { return Job.Cell == Cell; });
}

const FCytraWorker* FCytraMatch::FindWorker(int32 WorkerId) const
{
	return Workers.FindByPredicate([WorkerId](const FCytraWorker& Worker) { return Worker.Id == WorkerId; });
}

FCytraJob* FCytraMatch::FindJob(int32 JobId)
{
	return Jobs.FindByPredicate([JobId](const FCytraJob& Job) { return Job.Id == JobId; });
}

void FCytraMatch::AddWorker(FName Role)
{
	FCytraWorker& Worker = Workers.AddDefaulted_GetRef();
	Worker.Id = ++NextId;
	Worker.Role = Role;
	Worker.Position = FVector2d(Map.Camp.X, Map.Camp.Y);
}

int32 FCytraMatch::MinersAtMine() const
{
	int32 Count = 0;
	for (const FCytraWorker& Worker : Workers)
	{
		Count += Worker.State == ECytraWorkerState::ToMine || Worker.State == ECytraWorkerState::Mining ? 1 : 0;
	}
	return Count;
}

TOptional<TArray<FIntPoint>> FCytraMatch::PathFrom(const FCytraWorker& Worker, TFunctionRef<bool(const FIntPoint&)> Goal) const
{
	return Grid.FindPath(RoundToCell(Worker.Position), Goal);
}

void FCytraMatch::Go(FCytraWorker& Worker, TArray<FIntPoint> Path, ECytraWorkerState State) const
{
	const FIntPoint Cell = RoundToCell(Worker.Position);
	if (FMath::Abs(Worker.Position.X - Cell.X) + FMath::Abs(Worker.Position.Y - Cell.Y) > 0.01)
	{
		Path.Insert(Cell, 0);
	}
	Worker.MineTime = 0.0;
	Worker.State = Path.IsEmpty() ? Arrival(State) : State;
	Worker.Path = MoveTemp(Path);
}

void FCytraMatch::Manage()
{
	// 1. Waiting jobs, in order of command, take the nearest free worker of their specialty.
	for (FCytraJob& Job : Jobs)
	{
		if (Job.Worker != 0)
		{
			continue;
		}
		const FIntPoint Target = Job.Cell;
		FCytraWorker* Best = nullptr;
		TArray<FIntPoint> BestPath;
		int32 BestCost = MAX_int32;
		for (FCytraWorker& Worker : Workers)
		{
			if (!IsCytraWorkerFree(Worker.State) || Worker.Role != Job.Role)
			{
				continue;
			}
			TOptional<TArray<FIntPoint>> Path = PathFrom(Worker, [&Target](const FIntPoint& Cell) { return IsNextTo(Cell, Target); });
			if (!Path)
			{
				continue;
			}
			const bool bAtMine = Worker.State == ECytraWorkerState::Mining || Worker.State == ECytraWorkerState::ToMine;
			const int32 Cost = Path->Num() + (bAtMine ? MineRecallPenalty : 0);
			if (Cost < BestCost)
			{
				Best = &Worker;
				BestPath = MoveTemp(Path.GetValue());
				BestCost = Cost;
			}
		}
		if (Best)
		{
			Best->Job = Job.Id;
			Job.Worker = Best->Id;
			Go(*Best, MoveTemp(BestPath), ECytraWorkerState::Move);
		}
	}

	// 2. Without a job, a miner goes down an open mine with a free place; anyone else not on a camp walks to the nearest one.
	const FIntPoint* OpenMine = Map.Mines.FindByPredicate([this](const FIntPoint& Mine) { return Grid.IsOpened(Mine); });
	for (FCytraWorker& Worker : Workers)
	{
		if (Worker.State != ECytraWorkerState::Idle && Worker.State != ECytraWorkerState::Back)
		{
			continue;
		}
		if (OpenMine && Worker.Role == MineRole && MinersAtMine() < Balance.MineSlots)
		{
			const FIntPoint Mine = *OpenMine;
			TOptional<TArray<FIntPoint>> Path = PathFrom(Worker, [&Mine](const FIntPoint& Cell) { return IsNextTo(Cell, Mine); });
			if (Path)
			{
				Go(Worker, MoveTemp(Path.GetValue()), ECytraWorkerState::ToMine);
				continue;
			}
		}
		if (Worker.State == ECytraWorkerState::Idle && !Grid.IsCamp(RoundToCell(Worker.Position)))
		{
			TOptional<TArray<FIntPoint>> Path = PathFrom(Worker, [this](const FIntPoint& Cell) { return Grid.IsCamp(Cell); });
			if (Path)
			{
				Go(Worker, MoveTemp(Path.GetValue()), ECytraWorkerState::Back);
			}
		}
	}
}

void FCytraMatch::StepWorkers(double Dt)
{
	for (FCytraWorker& Worker : Workers)
	{
		if (IsWalking(Worker.State))
		{
			double Remaining = Balance.WorkerSpeed * Dt;
			while (Remaining > 0.0 && !Worker.Path.IsEmpty())
			{
				const FVector2d Target(Worker.Path[0].X, Worker.Path[0].Y);
				const FVector2d Delta = Target - Worker.Position;
				const double Distance = Delta.Length();
				if (Distance <= Remaining)
				{
					Worker.Position = Target;
					Worker.Path.RemoveAt(0);
					Remaining -= Distance;
				}
				else
				{
					Worker.Position += Delta / Distance * Remaining;
					Remaining = 0.0;
				}
			}
			if (Worker.Path.IsEmpty())
			{
				Worker.State = Arrival(Worker.State);
				Worker.MineTime = 0.0;
			}
		}
		else if (Worker.State == ECytraWorkerState::Work)
		{
			const int32 JobIndex = Jobs.IndexOfByPredicate([&Worker](const FCytraJob& Job) { return Job.Id == Worker.Job; });
			check(JobIndex != INDEX_NONE);
			Jobs[JobIndex].Progress += Dt;
			if (Jobs[JobIndex].Progress >= Jobs[JobIndex].Time)
			{
				Finish(JobIndex, Worker);
			}
		}
		// Mining income comes here (J50).
	}
}

void FCytraMatch::Finish(int32 JobIndex, FCytraWorker& Worker)
{
	const FCytraJob Job = Jobs[JobIndex];
	Jobs.RemoveAt(JobIndex);
	Worker.Job = 0;
	Worker.State = ECytraWorkerState::Idle;

	FCytraMatchEvent& Event = Events.AddDefaulted_GetRef();
	Event.Type = ECytraMatchEventType::JobDone;
	Event.Job = Job.Kind;
	Event.Work = Job.Type;
	Event.Cell = Job.Cell;

	switch (Job.Kind)
	{
	case ECytraOrderKind::Work:
	{
		const int32 Before = Grid.ReachableSlotCount();
		Grid.Open(Job.Cell);
		Event.OpenedSlots = Grid.ReachableSlotCount() - Before;
		break;
	}
	case ECytraOrderKind::Build:
	{
		Grid.PlaceTower(Job.Cell, Job.Type, Job.Cost);
		FCytraTowerGun& Gun = Towers.AddDefaulted_GetRef();
		Gun.Cell = Job.Cell;
		Event.Level = 1;
		break;
	}
	case ECytraOrderKind::Upgrade:
		Grid.UpgradeTower(Job.Cell, Job.Cost);
		Event.Level = Grid.GetTower(Job.Cell)->Level;
		break;
	case ECytraOrderKind::Dismantle:
	{
		const FCytraTower Tower = Grid.RemoveTower(Job.Cell);
		Towers.RemoveAll([&Job](const FCytraTowerGun& Gun) { return Gun.Cell == Job.Cell; });
		Event.Level = Tower.Level;
		Event.Refund = Balance.DismantleRefundOf(Tower.Invested);
		Wallet.EarnRefund(Event.Refund);
		break;
	}
	}
}

TArray<FCytraMatchEvent> FCytraMatch::ConsumeEvents()
{
	return MoveTemp(Events);
}
