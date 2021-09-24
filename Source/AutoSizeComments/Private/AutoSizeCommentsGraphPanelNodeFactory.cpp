// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "EdGraphNode_Comment.h"

TSharedPtr<SGraphNode> FAutoSizeCommentsGraphPanelNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (Cast<UEdGraphNode_Comment>(InNode))
	{
		const UAutoSizeCommentsSettings* ASCSettings = GetDefault<UAutoSizeCommentsSettings>();

		if (UEdGraph* Graph = InNode->GetGraph())
		{
			if (UClass* GraphClass = Graph->GetClass())
			{
				FString GraphClassName = GraphClass->GetName();

				if (ASCSettings->bDebugGraph_ASC)
				{
					UE_LOG(LogAutoSizeComments, Log, TEXT("GraphClassName: <%s>"), *GraphClassName);
				}

				TArray<FString> IgnoredClasses = ASCSettings->IgnoredGraphs;
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
