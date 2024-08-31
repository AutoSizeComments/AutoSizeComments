// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsNodeChangeData.h"

#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsUtils.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CreateDelegate.h"

#define DEBUG_CHANGE_DATA 0

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
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Node pos changed"));
#endif
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
#if DEBUG_CHANGE_DATA
				UE_LOG(LogTemp, Warning, TEXT("Pin changed"));
#endif
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
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Num pins changed"));
#endif
		return true;
	}

	if (AdvancedPinDisplay != (Node->AdvancedPinDisplay == ENodeAdvancedPins::Shown))
	{
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Advanced display changed"));
#endif
		return true;
	}

	if (NodeTitle != Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString())
	{
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Title changed"));
#endif
		return true;
	}

	if (bCommentBubblePinned != Node->bCommentBubblePinned)
	{
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Comment bubble pinned changed"));
#endif
		return true;
	}

	if (NodeEnabledState != Node->GetDesiredEnabledState())
	{
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Node enabled state changed"));
#endif
		return true;
	}

	if (UK2Node_CreateDelegate* Delegate = Cast<UK2Node_CreateDelegate>(Node))
	{
		if (DelegateFunctionName != Delegate->GetFunctionName())
		{
#if DEBUG_CHANGE_DATA
			UE_LOG(LogTemp, Warning, TEXT("Delegate function name changed"));
#endif
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
#if DEBUG_CHANGE_DATA
			UE_LOG(LogTemp, Warning, TEXT("Node deleted"));
#endif
			LastNodes.RemoveAt(i);
		}
	}

	// check if node has been added or removed
	if (Comment->GetNodesUnderComment().Num() != LastNodes.Num())
	{
#if DEBUG_CHANGE_DATA
		UE_LOG(LogTemp, Warning, TEXT("Node added or removed!"));
#endif
		return true;
	}

	for (UObject* Node : Comment->GetNodesUnderComment())
	{
		if (!LastNodes.Contains(Node))
		{
#if DEBUG_CHANGE_DATA
			UE_LOG(LogTemp, Warning, TEXT("Node added"));
#endif
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
#if DEBUG_CHANGE_DATA
				UE_LOG(LogTemp, Warning, TEXT("Data has changed!"));
#endif
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
