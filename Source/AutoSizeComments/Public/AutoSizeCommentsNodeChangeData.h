// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UEdGraphNode_Comment;

struct FASCPinChangeData
{
	bool bPinLinked;
	bool bPinHidden;
	FString PinValue;
	FText PinTextValue;
	FText PinLabel;
	FString PinObject;

	FASCPinChangeData() = default;

	void UpdatePin(UEdGraphPin* Pin);

	bool HasPinChanged(UEdGraphPin* Pin);

	FString GetPinDefaultObjectName(UEdGraphPin* Pin) const;

	FText GetPinLabel(UEdGraphPin* Pin) const;
};


/**
 * @brief Node can change by:
 *		- Pin being linked
 *		- Pin value changing
 *		- Pin being added or removed
 *		- Expanding the node (see print string)
 *		- Node title changing
 *		- Comment bubble pinned
 */
class FASCNodeChangeData
{
	TMap<FGuid, FASCPinChangeData> PinChangeData;
	bool bCommentBubblePinned;
	FString NodeTitle;
	bool AdvancedPinDisplay;
	ENodeEnabledState NodeEnabledState;
	int32 NodeX;
	int32 NodeY;
	FName DelegateFunctionName;

public:
	FASCNodeChangeData() = default;

	void UpdateNode(UEdGraphNode* Node);

	bool HasNodeChanged(UEdGraphNode* Node);
};

class FASCCommentChangeData
{
	FString NodeComment;
	TMap<TWeakObjectPtr<UEdGraphNode>, FASCNodeChangeData> NodeChangeData;

public:
	FASCCommentChangeData() = default;

	void UpdateComment(UEdGraphNode_Comment* Comment);

	bool HasCommentChanged(UEdGraphNode_Comment* Comment);
};