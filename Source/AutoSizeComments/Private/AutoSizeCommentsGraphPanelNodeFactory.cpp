// Copyright 2020 fpwong, Inc. All Rights Reserved.

#include "AutoSizeCommentsGraphPanelNodeFactory.h"
#include "AutoSizeCommentsGraphNode.h"
#include "EdGraphNode_Comment.h"

TSharedPtr<SGraphNode> FAutoSizeCommentsGraphPanelNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (Cast<UEdGraphNode_Comment>(InNode))
	{
		return SNew(SAutoSizeCommentsGraphNode, InNode);
	}

	return nullptr;
}
