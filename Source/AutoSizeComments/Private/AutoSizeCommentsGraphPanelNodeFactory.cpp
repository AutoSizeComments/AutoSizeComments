// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphPanelNodeFactory.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "Editor/Transactor.h"
#include "Framework/Commands/GenericCommands.h"

TSharedPtr<SGraphNode> FAutoSizeCommentsGraphPanelNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (!InNode)
	{
		return nullptr;
	}

	// check if the node was copy-pasted by checking if it was in the last transaction and it matches the undo description
	bool bWasCopyPasted = false;
	if (GEditor && GEditor->Trans)
	{
		const int Index = GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1;
		if (const FTransaction* LastTransaction = GEditor->Trans->GetTransaction(Index))
		{
			const FTransactionContext Context = LastTransaction->GetContext();

			if (LastTransaction->ContainsObject(InNode))
			{
				if (Context.Title.EqualTo(FGenericCommands::Get().Paste->GetDescription()))
				{
					bWasCopyPasted = true;
					// UE_LOG(LogTemp, Warning, TEXT("Last transaction Context %s Title %s"), *Context.Context, *Context.Title.ToString());
				}
			}
		}
	}

	// init graph handler for containing graph
	FAutoSizeCommentGraphHandler::Get().BindToGraph(InNode->GetGraph());

	const FASCGraphHandlerData& GraphData = FAutoSizeCommentGraphHandler::Get().GetGraphHandlerData(InNode->GetGraph());

	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	if (ASCSettings.bDisableASCGraphNode)
	{
		return nullptr;
	}

	if (Cast<UEdGraphNode_Comment>(InNode))
	{
		if (UEdGraph* Graph = InNode->GetGraph())
		{
			if (UClass* GraphClass = Graph->GetClass())
			{
				FString GraphClassName = GraphClass->GetName();

				if (ASCSettings.bDebugGraph_ASC)
				{
					UE_LOG(LogAutoSizeComments, Log, TEXT("GraphClassName: <%s>"), *GraphClassName);
				}

				TArray<FString> IgnoredClasses = ASCSettings.IgnoredGraphs;
				for (FString Ignored : IgnoredClasses)
				{
					if (GraphClassName == Ignored)
					{
						return nullptr;
					}
				}
			}

			TSharedRef<SAutoSizeCommentsGraphNode> GraphNode = SNew(SAutoSizeCommentsGraphNode, InNode);
			GraphNode->InitialSelection = GraphData.PreviousSelection;
			GraphNode->bWasCopyPasted = bWasCopyPasted;
			return GraphNode;
		}
	}

	return nullptr;
}
