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
	FireTowers(Dt);
	MoveShots(Dt);
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
		Enemy.Distance += Enemy.SpeedAt(Time) * Dt;
		if (Enemy.Distance >= Path.GetLength())
		{
			// Reaching the nest destroys eggs but pays nothing.
			Enemy.bDead = true;
			Eggs = FMath::Max(0, Eggs - Enemy.Eggs);
			FCytraMatchEvent& Event = Events.AddDefaulted_GetRef();
			Event.Type = ECytraMatchEventType::Leak;
			Event.Enemy = Enemy.Type;
			Event.Eggs = Enemy.Eggs;
			FCytraEffect& Effect = Effects.AddDefaulted_GetRef();
			Effect.Kind = ECytraEffectKind::Leak;
			Effect.To = FVector2d(Map.Nest.X, Map.Nest.Y);
		}
		else
		{
			Enemy.Position = Path.PositionAt(Enemy.Distance);
		}
	}
}

void FCytraMatch::DamageEnemy(int32 EnemyId, double Damage)
{
	if (FCytraEnemy* Enemy = Enemies.FindByPredicate([EnemyId](const FCytraEnemy& E) { return E.Id == EnemyId; }))
	{
		Hit(*Enemy, Damage);
	}
}

void FCytraMatch::Hit(FCytraEnemy& Enemy, double Damage)
{
	if (Enemy.bDead)
	{
		return;
	}
	Enemy.Hp -= Damage;
	if (Enemy.Hp <= 0.0)
	{
		Enemy.bDead = true;
		Wallet.Earn(Enemy.Gold);
		++Kills;
		FCytraEffect& Effect = Effects.AddDefaulted_GetRef();
		Effect.Kind = ECytraEffectKind::Loot;
		Effect.To = Enemy.Position;
		Effect.Gold = Enemy.Gold;
	}
}

void FCytraMatch::FireTowers(double Dt)
{
	TArray<FCytraEnemy*> InRange;
	for (FCytraTowerGun& Gun : Towers)
	{
		Gun.Cooldown -= Dt;
		if (Gun.Cooldown > 0.0)
		{
			continue;
		}
		const FCytraTower& Tower = *Grid.GetTower(Gun.Cell);
		const FCytraTowerDef& Def = Balance.Towers.FindChecked(Tower.Type);
		const FCytraTowerStats Stats = Balance.TowerStats(Tower.Type, Tower.Level, Grid.GetType(Gun.Cell) == ECytraTileType::HighSlot);
		const FVector2d Origin(Gun.Cell.X, Gun.Cell.Y);

		InRange.Reset();
		for (FCytraEnemy& Enemy : Enemies)
		{
			if (!Enemy.bDead && Enemy.Position.X >= Balance.TargetingMinX && FVector2d::Distance(Enemy.Position, Origin) <= Stats.Range)
			{
				InRange.Add(&Enemy);
			}
		}
		if (InRange.IsEmpty())
		{
			// Without a target the tower stays ready.
			Gun.Cooldown = 0.0;
			continue;
		}
		// Most advanced first. Stable, like the sort of the prototype.
		InRange.StableSort([](const FCytraEnemy& A, const FCytraEnemy& B) { return A.Distance > B.Distance; });

		FCytraEnemy& Target = *InRange[0];
		const FVector2d ToTarget = Target.Position - Origin;
		Gun.Aim = FMath::Atan2(ToTarget.Y, ToTarget.X);
		Gun.Cooldown = 1.0 / Stats.Rate;

		switch (Def.Behavior)
		{
		case ECytraTowerBehavior::Bolt:
		{
			FCytraShot& Shot = Shots.AddDefaulted_GetRef();
			Shot.Kind = ECytraShotKind::Bolt;
			Shot.Tower = Tower.Type;
			Shot.Damage = Stats.Damage;
			Shot.Position = Origin;
			Shot.Angle = Gun.Aim;
			Shot.Speed = Def.BoltSpeed;
			Shot.Target = Target.Id;
			break;
		}
		case ECytraTowerBehavior::Shell:
		{
			// Aimed where the target will be when the shell lands, at its current speed.
			const FCytraRoutePath& Path = Routes[Target.Route];
			FCytraShot& Shot = Shots.AddDefaulted_GetRef();
			Shot.Kind = ECytraShotKind::Shell;
			Shot.Tower = Tower.Type;
			Shot.Damage = Stats.Damage;
			Shot.From = Origin;
			Shot.To = Path.PositionAt(FMath::Min(Path.GetLength(), Target.Distance + Target.SpeedAt(Time) * Def.FlightTime));
			Shot.Duration = Def.FlightTime;
			Shot.Splash = Def.Splash;
			break;
		}
		case ECytraTowerBehavior::Chain:
			for (int32 Index = 0; Index < FMath::Min(Def.Targets, InRange.Num()); ++Index)
			{
				// The slow does not stack: a new hit only restarts it.
				FCytraEnemy& Victim = *InRange[Index];
				Victim.SlowUntil = Time + Def.SlowTime;
				Victim.SlowFactor = Def.Slow;
				FCytraEffect& Effect = Effects.AddDefaulted_GetRef();
				Effect.Kind = ECytraEffectKind::Zap;
				Effect.Tower = Tower.Type;
				Effect.From = Origin;
				Effect.To = Victim.Position;
				Hit(Victim, Stats.Damage);
			}
			break;
		}
	}
}

void FCytraMatch::MoveShots(double Dt)
{
	for (FCytraShot& Shot : Shots)
	{
		if (Shot.Kind == ECytraShotKind::Bolt)
		{
			// A bolt whose target died before the impact vanishes without damage.
			FCytraEnemy* Target = Enemies.FindByPredicate([&Shot](const FCytraEnemy& Enemy) { return Enemy.Id == Shot.Target; });
			if (!Target || Target->bDead)
			{
				Shot.bDone = true;
				continue;
			}
			const FVector2d Delta = Target->Position - Shot.Position;
			const double Distance = Delta.Length();
			const double Move = Shot.Speed * Dt;
			Shot.Angle = FMath::Atan2(Delta.Y, Delta.X);
			if (Distance <= Move)
			{
				Hit(*Target, Shot.Damage);
				Shot.bDone = true;
			}
			else
			{
				Shot.Position += Delta / Distance * Move;
			}
		}
		else
		{
			Shot.Elapsed += Dt;
			if (Shot.Elapsed >= Shot.Duration)
			{
				Shot.bDone = true;
				for (FCytraEnemy& Enemy : Enemies)
				{
					if (!Enemy.bDead && FVector2d::Distance(Enemy.Position, Shot.To) <= Shot.Splash)
					{
						Hit(Enemy, Shot.Damage);
					}
				}
				FCytraEffect& Effect = Effects.AddDefaulted_GetRef();
				Effect.Kind = ECytraEffectKind::Blast;
				Effect.Tower = Shot.Tower;
				Effect.To = Shot.To;
				Effect.Radius = Shot.Splash;
			}
		}
	}
	Shots.RemoveAll([](const FCytraShot& Shot) { return Shot.bDone; });
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

FCytraRecruitCheck FCytraMatch::CheckRecruit(FName Role) const
{
	FCytraRecruitCheck Result;
	Result.MaxCrew = Balance.MaxCrew;
	const FCytraRoleDef* Def = Balance.Roles.Find(Role);
	if (!Def)
	{
		Result.Refusal = ECytraRecruitRefusal::UnknownRole;
		return Result;
	}
	Result.Cost = Def->RecruitCost;
	if (Workers.Num() >= Balance.MaxCrew)
	{
		Result.Refusal = ECytraRecruitRefusal::CrewFull;
	}
	else if (!Wallet.CanAfford(Result.Cost))
	{
		Result.Refusal = ECytraRecruitRefusal::NotEnoughGold;
		Result.MissingGold = Wallet.Missing(Result.Cost);
	}
	return Result;
}

FCytraRecruitCheck FCytraMatch::Recruit(FName Role)
{
	FCytraRecruitCheck Result = CheckRecruit(Role);
	if (!Result.IsAccepted())
	{
		return Result;
	}
	Wallet.TrySpend(Result.Cost);
	AddWorker(Role);
	Result.Worker = Workers.Last().Id;
	Manage();
	return Result;
}

FCytraDismissCheck FCytraMatch::CheckDismiss(int32 WorkerId) const
{
	FCytraDismissCheck Result;
	const FCytraWorker* Worker = FindWorker(WorkerId);
	if (IsOver())
	{
		Result.Refusal = ECytraDismissRefusal::GameOver;
	}
	else if (!Worker)
	{
		Result.Refusal = ECytraDismissRefusal::UnknownWorker;
	}
	if (Worker)
	{
		Result.Role = Worker->Role;
		Result.Job = Worker->Job;
		const FCytraRoleDef* Def = Balance.Roles.Find(Worker->Role);
		// The margin keeps a product such as 50 x 0.7 from rounding down to 34.
		Result.Refund = Def ? FMath::FloorToInt32(Def->RecruitCost * Balance.DismissRefund + 1e-6) : 0;
	}
	return Result;
}

FCytraDismissCheck FCytraMatch::Dismiss(int32 WorkerId)
{
	const FCytraDismissCheck Result = CheckDismiss(WorkerId);
	if (!Result.IsAccepted())
	{
		return Result;
	}
	if (FCytraJob* Job = FindJob(Result.Job))
	{
		Job->Worker = 0;
	}
	Workers.RemoveAll([WorkerId](const FCytraWorker& Worker) { return Worker.Id == WorkerId; });
	Wallet.EarnRefund(Result.Refund);
	Manage();
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

int32 FCytraMatch::MinersMining() const
{
	return Workers.FilterByPredicate([](const FCytraWorker& Worker) { return Worker.State == ECytraWorkerState::Mining; }).Num();
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
		else if (Worker.State == ECytraWorkerState::Mining)
		{
			Worker.MineTime += Dt;
			if (Worker.MineTime >= Balance.MinePeriod)
			{
				Worker.MineTime -= Balance.MinePeriod;
				Wallet.EarnFromMine(Balance.MineYield);
				FCytraMatchEvent& Event = Events.AddDefaulted_GetRef();
				Event.Type = ECytraMatchEventType::MineYield;
				Event.Gold = Balance.MineYield;
				if (const FIntPoint* Mine = Map.Mines.FindByPredicate([this](const FIntPoint& Cell) { return Grid.IsOpened(Cell); }))
				{
					Event.Cell = *Mine;
				}
				FCytraEffect& Effect = Effects.AddDefaulted_GetRef();
				Effect.Kind = ECytraEffectKind::MineGold;
				Effect.To = FVector2d(Event.Cell.X, Event.Cell.Y);
				Effect.Gold = Event.Gold;
			}
		}
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

TArray<FCytraEffect> FCytraMatch::ConsumeEffects()
{
	return MoveTemp(Effects);
}
