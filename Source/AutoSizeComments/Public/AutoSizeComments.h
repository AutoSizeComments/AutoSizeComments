// Copyright 2018 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "AutoSizeGraphNodeFactory.h"

class FAutoSizeCommentsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FAutoSizeGraphNodeFactory> AutoSizeGraphNodeFactory;
};
