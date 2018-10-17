// Copyright 2018 fpwong, Inc. All Rights Reserved.

#include "AutoSizeGraphNodeFactory.h"
#include "EdGraphNode_Comment.h"
#include "AutoSizeCommentNode.h"

TSharedPtr<class SGraphNode> FAutoSizeGraphNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (Cast<UEdGraphNode_Comment>(InNode))
	{
		return SNew(SAutoSizeCommentNode, InNode);
	}

	return nullptr;
}
