// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CytraMessages.h"

#include "Rules/CytraMatch.h"
#include "UI/CytraPanelView.h"
#include "UI/CytraText.h"

namespace
{
	FCytraMessage Good(const FText& Text)
	{
		return FCytraMessage{ Text, ECytraMessageTone::Good };
	}

	FCytraMessage Warning(const FText& Text)
	{
		return FCytraMessage{ Text, ECytraMessageTone::Warning };
	}
}

TOptional<FCytraMessage> FCytraMessages::FromEvent(const FCytraMatchEvent& Event)
{
	switch (Event.Type)
	{
	case ECytraMatchEventType::Leak:
		return Warning(FText::Format(CytraText::Get(TEXT("Message.Leak")), CytraText::Data(TEXT("Enemy"), Event.Enemy, TEXT("One")), Event.Eggs));
	case ECytraMatchEventType::WaveCleared:
		return Good(FText::Format(CytraText::Get(TEXT("Message.WaveCleared")), Event.Wave, Event.Gold));
	case ECytraMatchEventType::JobDone:
		switch (Event.Job)
		{
		case ECytraOrderKind::Work:
		{
			const FText Done = CytraText::Data(TEXT("Work"), Event.Work, TEXT("Done"));
			return Good(Event.OpenedSlots > 0 ? FText::Format(CytraText::Get(TEXT("Message.Opened")), Done, Event.OpenedSlots) : Done);
		}
		case ECytraOrderKind::Build:
			return Good(FText::Format(CytraText::Get(TEXT("Message.TowerBuilt")), CytraText::Data(TEXT("Tower"), Event.Work, TEXT("Name"))));
		case ECytraOrderKind::Upgrade:
			return Good(FText::Format(CytraText::Get(TEXT("Message.TowerUpgraded")), CytraText::Data(TEXT("Tower"), Event.Work, TEXT("Name")), Event.Level));
		case ECytraOrderKind::Dismantle:
			return Good(FText::Format(CytraText::Get(TEXT("Message.Dismantled")), CytraText::Data(TEXT("Tower"), Event.Work, TEXT("Name")), Event.Refund));
		}
		break;
	case ECytraMatchEventType::MineYield:
		break;
	}
	return {};
}

FCytraMessage FCytraMessages::FromOrder(const FCytraOrderResult& Result)
{
	if (!Result.IsAccepted() || !Result.Check.Spec)
	{
		return Warning(FCytraPanelView::RefusalText(Result.Check));
	}
	const FCytraOrderSpec& Spec = *Result.Check.Spec;
	FText Text;
	switch (Spec.Kind)
	{
	case ECytraOrderKind::Work:
		Text = FText::Format(CytraText::Get(TEXT("Message.Ordered")), CytraText::Data(TEXT("Work"), Spec.Id, TEXT("Label")), Spec.Cost);
		break;
	case ECytraOrderKind::Build:
		Text = FText::Format(CytraText::Get(TEXT("Message.Ordered")), CytraText::Data(TEXT("Tower"), Spec.Id, TEXT("Name")), Spec.Cost);
		break;
	case ECytraOrderKind::Upgrade:
		Text = FText::Format(CytraText::Get(TEXT("Message.Ordered")), CytraText::Get(TEXT("Message.Upgrade")), Spec.Cost);
		break;
	case ECytraOrderKind::Dismantle:
		Text = FText::Format(CytraText::Get(TEXT("Message.DismantleOrdered")), Spec.Refund);
		break;
	}
	if (Result.bWaiting)
	{
		Text = FText::Format(CytraText::Get(TEXT("Message.Waiting")), Text, CytraText::Data(TEXT("Role"), Spec.Role, TEXT("Short")));
	}
	return Good(Text);
}

FCytraMessage FCytraMessages::FromRecruit(FName Role, const FCytraRecruitCheck& Check)
{
	if (!Check.IsAccepted())
	{
		return Warning(FCytraPanelView::RecruitRefusalText(Check));
	}
	return Good(FText::Format(CytraText::Get(TEXT("Message.Recruited")), CytraText::Data(TEXT("Role"), Role, TEXT("Name"))));
}

TOptional<FCytraMessage> FCytraMessages::FromLaunch(const FCytraLaunchResult& Result)
{
	if (!Result.bLaunched || Result.Bonus <= 0)
	{
		return {};
	}
	return Good(FText::Format(CytraText::Get(TEXT("Message.EarlyLaunch")), Result.Bonus));
}
