// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rules/CytraMatch.h"

FCytraMatch::FCytraMatch(const FCytraMapDef& InMap, const FCytraBalance& InBalance, const FCytraWaveSchedule& InWaves)
	: Map(InMap)
	, Balance(InBalance)
	, Waves(InWaves)
	, Wallet(InBalance.StartGold)
	, Eggs(InBalance.StartEggs)
{
	for (const FCytraRoute& Route : Map.Routes)
	{
		Routes.Emplace(Route);
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
	// Workers and job assignment come here (J30).

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

TArray<FCytraMatchEvent> FCytraMatch::ConsumeEvents()
{
	return MoveTemp(Events);
}
