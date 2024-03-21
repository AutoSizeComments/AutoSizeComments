// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutoSizeCommentsMacros.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
 * 
 */
class AUTOSIZECOMMENTS_API FASCCommands final : public TCommands<FASCCommands>
{
public:
	FASCCommands()
		: TCommands<FASCCommands>(
			TEXT("AutoSizeCommentsCommands"),
			INVTEXT("Auto Size Comments Commands"),
			NAME_None,
			ASC_GET_STYLE_SET_NAME()) { }

	virtual ~FASCCommands() override { }

	virtual void RegisterCommands() override;
};
