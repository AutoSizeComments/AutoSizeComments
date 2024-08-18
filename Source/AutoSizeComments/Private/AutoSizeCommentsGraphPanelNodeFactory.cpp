// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphPanelNodeFactory.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraph.h"

TSharedPtr<SGraphNode> FAutoSizeCommentsGraphPanelNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (InNode)
	{
		// init graph handler for containing graph
		FAutoSizeCommentGraphHandler::Get().BindToGraph(InNode->GetGraph());
	}

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
		}

		TSharedRef<SAutoSizeCommentsGraphNode> GraphNode = SNew(SAutoSizeCommentsGraphNode, InNode);
		GraphNode->SlatePrepass();
		return GraphNode;
	}

	return nullptr;
}
