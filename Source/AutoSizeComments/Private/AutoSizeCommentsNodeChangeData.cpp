﻿// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsNodeChangeData.h"

#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CreateDelegate.h"

void FASCPinChangeData::UpdatePin(UEdGraphPin* Pin)
{
	bPinHidden = Pin->bHidden;
	bPinLinked = Pin->LinkedTo.Num() > 0;
	PinValue = Pin->DefaultValue;
	PinTextValue = Pin->DefaultTextValue;
	PinLabel = GetPinLabel(Pin);
	PinObject = GetPinDefaultObjectName(Pin);
}

bool FASCPinChangeData::HasPinChanged(UEdGraphPin* Pin)
{
	if (bPinHidden != Pin->bHidden)
	{
		return true;
	}

	if (bPinLinked != (Pin->LinkedTo.Num() > 0))
	{
		// these pins do not change size
		if (Pin->PinType.PinSubCategory != UEdGraphSchema_K2::PC_Exec)
		{
			return true;
		}
	}

	if (PinValue != Pin->DefaultValue)
	{
		return true;
	}

	if (!PinTextValue.EqualTo(Pin->DefaultTextValue, ETextComparisonLevel::Default))
	{
		return true;
	}

	if (!PinLabel.EqualTo(GetPinLabel(Pin), ETextComparisonLevel::Default))
	{
		return true;
	}

	const FString PinDefaultObjectName = GetPinDefaultObjectName(Pin);
	if (PinObject != PinDefaultObjectName)
	{
		return true;
	}


	return false;
}

FString FASCPinChangeData::GetPinDefaultObjectName(UEdGraphPin* Pin) const
{
	return Pin->DefaultObject ? Pin->DefaultObject->GetName() : FString();
}

FText FASCPinChangeData::GetPinLabel(UEdGraphPin* Pin) const
{
	if (Pin)
	{
		if (UEdGraphNode* GraphNode = Pin->GetOwningNodeUnchecked())
		{
			return GraphNode->GetPinDisplayName(Pin);
		}
	}

	return FText::GetEmpty();
}

void FASCNodeChangeData::UpdateNode(UEdGraphNode* Node)
{
	PinChangeData.Reset();
	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		PinChangeData.FindOrAdd(Pin->PinId).UpdatePin(Pin);
	}

	AdvancedPinDisplay = Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown;
	NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	bCommentBubblePinned = Node->bCommentBubblePinned;
	NodeEnabledState = Node->GetDesiredEnabledState();
	NodeX = Node->NodePosX;
	NodeY = Node->NodePosY;

	if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
	{
		DelegateFunctionName = Delegate->GetFunctionName();
	}
}

bool FASCNodeChangeData::HasNodeChanged(UEdGraphNode* Node)
{
	if (Node->NodePosX != NodeX || Node->NodePosY != NodeY)
	{
		return true;
	}

	TArray<FGuid> PinGuids;
	PinChangeData.GetKeys(PinGuids);

	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		if (FASCPinChangeData* FoundPinData = PinChangeData.Find(Pin->PinId))
		{
			if (FoundPinData->HasPinChanged(Pin))
			{
				return true;
			}

			PinGuids.Remove(Pin->PinId);
		}
		else // added a new pin
		{
			return true;
		}
	}

	// If there are remaining pins, then they must have been removed
	if (PinGuids.Num())
	{
		return true;
	}

	if (AdvancedPinDisplay != (Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown))
	{
		return true;
	}

	if (NodeTitle != Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString())
	{
		return true;
	}

	if (bCommentBubblePinned != Node->bCommentBubblePinned)
	{
		return true;
	}

	if (NodeEnabledState != Node->GetDesiredEnabledState())
	{
		return true;
	}

	if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
	{
		if (DelegateFunctionName != Delegate->GetFunctionName())
		{
			return true;
		}
	}

	return false;
}

void FASCCommentChangeData::UpdateComment(UEdGraphNode_Comment* Comment)
{
	NodeChangeData.Reset();
	for (UObject* Obj : Comment->GetNodesUnderComment())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			NodeChangeData.FindOrAdd(Node).UpdateNode(Node);
		}
	}

	NodeComment = Comment->NodeComment;
}

bool FASCCommentChangeData::HasCommentChanged(UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		return false;
	}

	UEdGraph* Graph = Comment->GetGraph();
	if (!Graph)
	{
		return false;
	}

	if (!NodeComment.Equals(Comment->NodeComment, ESearchCase::Type::CaseSensitive))
	{
		return true;
	}

	TArray<TWeakObjectPtr<UEdGraphNode>> LastNodes;
	NodeChangeData.GetKeys(LastNodes);

	// remove all deleted / invalid nodes
	for (int i = LastNodes.Num() - 1; i >= 0; --i)
	{
		TWeakObjectPtr<UEdGraphNode> Node = LastNodes[i];
		if (!Node.IsValid() || !Graph->Nodes.Contains(Node))
		{
			// UE_LOG(LogTemp, Warning, TEXT("Node deleted"));
			LastNodes.RemoveAt(i);
		}
	}

	// check if node has been added or removed
	if (Comment->GetNodesUnderComment().Num() != LastNodes.Num())
	{
		// UE_LOG(LogTemp, Warning, TEXT("Node added or removed!"));
		return true;
	}

	for (UObject* Node : Comment->GetNodesUnderComment())
	{
		if (!LastNodes.Contains(Node))
		{
			// UE_LOG(LogTemp, Warning, TEXT("Node added"));
			return true;
		}
	}

	for (TWeakObjectPtr<UEdGraphNode> Node : LastNodes)
	{
		if (Node.IsValid())
		{
			FASCNodeChangeData* Data = NodeChangeData.Find(Node);
			if (Data && Data->HasNodeChanged(Node.Get()))
			{
				// UE_LOG(LogTemp, Warning, TEXT("Data has changed!"));
				return true;
			}
		}
	}

	return false;
}

void FASCCommentChangeData::DebugPrint()
{
	UE_LOG(LogAutoSizeComments, Log, TEXT("%s"), *NodeComment);
	for (auto& Elem : NodeChangeData)
	{
		if (Elem.Key.IsValid())
		{
			UE_LOG(LogAutoSizeComments, Log, TEXT("\t%s"), *FASCUtils::GetNodeName(Elem.Key.Get()));
		}
	}
}
