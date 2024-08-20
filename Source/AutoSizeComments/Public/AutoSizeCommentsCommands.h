// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutoSizeCommentsMacros.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class FUICommandList;

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

	TSharedPtr<FUICommandInfo> ResizeSelectedComment;

	static TSharedPtr<FUICommandList> MakeCommandList();

	static void RunResizeSelectedComment();
};
