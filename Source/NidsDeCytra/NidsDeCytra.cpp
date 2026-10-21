// Copyright Epic Games, Inc. All Rights Reserved.

#include "NidsDeCytra.h"
#include "Modules/ModuleManager.h"
#include "UI/CytraText.h"

class FNidsDeCytraModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		CytraText::Register();
	}

	virtual void ShutdownModule() override
	{
		CytraText::Unregister();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FNidsDeCytraModule, NidsDeCytra, "NidsDeCytra" );
