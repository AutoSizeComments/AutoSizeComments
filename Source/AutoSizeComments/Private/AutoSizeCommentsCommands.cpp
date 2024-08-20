// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsCommands.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsState.h"
#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "AutoSizeCommentsCommands"

void FASCCommands::RegisterCommands()
{
	UI_COMMAND(
		ResizeSelectedComment,
		"Resize Selected Comment",
		"Resize the selected comment to fit the nodes inside",
		EUserInterfaceActionType::Button,
		FInputChord());
}

TSharedPtr<FUICommandList> FASCCommands::MakeCommandList()
{
	TSharedPtr<FUICommandList> OutCommandList = MakeShareable(new FUICommandList());

	OutCommandList->MapAction(
		FASCCommands::Get().ResizeSelectedComment,
		FExecuteAction::CreateStatic(FASCCommands::RunResizeSelectedComment)
	);

	return OutCommandList;
}

void FASCCommands::RunResizeSelectedComment()
{
	for (TSharedPtr<SGraphPanel> GraphPanel : FAutoSizeCommentGraphHandler::Get().GetActiveGraphPanels())
	{
		for (UObject* SelectedGraphNode : GraphPanel->SelectionManager.GetSelectedNodes())
		{
			if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(SelectedGraphNode))
			{
				if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCNode = FASCState::Get().GetASCComment(SelectedComment))
				{
					ASCNode->ResizeToFit();
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
