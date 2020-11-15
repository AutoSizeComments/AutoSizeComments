// Copyright 2020 fpwong, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

class SGraphNode;

class AUTOSIZECOMMENTS_API FAutoSizeCommentsGraphPanelNodeFactory : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<SGraphNode> CreateNode(class UEdGraphNode* Node) const override;
};
