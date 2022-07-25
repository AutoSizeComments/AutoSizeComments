#include "AutoSizeCommentsUtils.h"

#include "AutoSizeCommentsCacheFile.h"
#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "Kismet2/BlueprintEditorUtils.h"

TArray<UEdGraphNode_Comment*> FASCUtils::GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments, UEdGraphNode* Node)
{
	TArray<UEdGraphNode_Comment*> ContainingComments;
	for (auto Comment : Comments)
	{
		TArray<UEdGraphNode*> NodesUnderComments = GetNodesUnderComment(Comment);
		if (NodesUnderComments.Contains(Node))
		{
			ContainingComments.Add(Comment);
		}
	}

	return ContainingComments;
}

TArray<UEdGraphNode*> FASCUtils::GetNodesUnderComment(UEdGraphNode_Comment* Comment)
{
	TArray<UEdGraphNode*> OutNodes;
	for (UObject* Obj : Comment->GetNodesUnderComment())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			OutNodes.Add(Node);
		}
	}

	return OutNodes;
}

TArray<UEdGraphPin*> FASCUtils::GetPinsByDirection(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	const auto Pred = [Direction](UEdGraphPin* Pin)
	{
		return !Pin->bHidden && (Pin->Direction == Direction || Direction == EGPD_MAX);
	};

	return Node->Pins.FilterByPredicate(Pred);
}

TArray<UEdGraphPin*> FASCUtils::GetLinkedPins(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	const auto Pred = [](UEdGraphPin* Pin)
	{
		return Pin->LinkedTo.Num() > 0;
	};

	return GetPinsByDirection(Node, Direction).FilterByPredicate(Pred);
}

TArray<UEdGraphNode*> FASCUtils::GetLinkedNodes(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
{
	TSet<UEdGraphNode*> Nodes;
	for (UEdGraphPin* LinkedPin : GetLinkedPins(Node, Direction))
	{
		if (LinkedPin->LinkedTo.Num() > 0)
		{
			for (UEdGraphPin* Linked2LinkedPin : LinkedPin->LinkedTo)
			{
				Nodes.Add(Linked2LinkedPin->GetOwningNode());
			}
		}
	}

	return Nodes.Array();
}

bool FASCUtils::HasNodeBeenDeleted(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return true;
	}

	UEdGraph* Graph = Node->GetGraph();
	if (!Graph)
	{
		return true;
	}

	return !Graph->Nodes.Contains(Node);
}

bool FASCUtils::IsValidPin(UEdGraphPin* Pin)
{
	return Pin != nullptr && !Pin->bWasTrashed;
}

TSharedPtr<SGraphPin> FASCUtils::GetGraphPin(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin)
{
	if (!IsValidPin(Pin) || !GraphPanel.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphNode> GraphNode = GetGraphNode(GraphPanel, Pin->GetOwningNode());
	return GraphNode.IsValid() ? GraphNode->FindWidgetForPin(Pin) : nullptr;
}

TSharedPtr<SGraphNode> FASCUtils::GetGraphNode(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphNode* Node)
{
	if (Node == nullptr || !GraphPanel.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<SGraphNode> GraphNode_GUID = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
	if (GraphNode_GUID.IsValid())
	{
		return GraphNode_GUID;
	}

	FChildren* Children = GraphPanel->GetChildren();
	for (int i = 0; i < Children->Num(); i++)
	{
		TSharedPtr<SGraphNode> GraphNode = StaticCastSharedRef<SGraphNode>(Children->GetChildAt(i));

		if (GraphNode.IsValid())
		{
			if (GraphNode->GetNodeObj() == Node)
			{
				return GraphNode;
			}
		}
	}

	return nullptr;
}

TSharedPtr<SGraphPin> FASCUtils::GetHoveredGraphPin(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (!GraphPanel.IsValid())
	{
		return nullptr;
	}

	UEdGraph* Graph = GraphPanel->GetGraphObj();
	if (Graph == nullptr)
	{
		return nullptr;
	}

	const bool bIsMaterialGraph = Graph->GetClass()->GetFName() == "MaterialGraph";

	// check if graph pin "IsHovered" function
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin->bHidden)
			{
				TSharedPtr<SGraphPin> GraphPin = GetGraphPin(GraphPanel, Pin);
				if (GraphPin.IsValid())
				{
					const bool bIsHovered = bIsMaterialGraph ? GraphPin->IsDirectlyHovered() : GraphPin->IsHovered();
					if (bIsHovered)
					{
						return GraphPin;
					}
				}
			}
		}
	}

	return nullptr;
}

TArray<UEdGraphNode_Comment*> FASCUtils::GetSelectedComments(TSharedPtr<SGraphPanel> GraphPanel)
{
	TArray<UEdGraphNode_Comment*> OutComments;
	if (!GraphPanel)
		return OutComments;

	for (UObject* SelectedObj : GraphPanel->SelectionManager.SelectedNodes)
	{
		if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(SelectedObj))
		{
			OutComments.Add(SelectedComment);
		}
	}

	return OutComments;
}

bool FASCUtils::IsGraphReadOnly(TSharedPtr<SGraphPanel> GraphPanel)
{
	if (GraphPanel)
	{
		if (!GraphPanel->IsGraphEditable())
		{
			return true;
		}

		if (UEdGraph* Graph = GraphPanel->GetGraphObj())
		{
			return FBlueprintEditorUtils::IsGraphReadOnly(Graph);
		}
	}

	return false;
}

FString FASCUtils::GetNodeName(UEdGraphNode* Node)
{
	if (Node == nullptr)
	{
		return FString("nullptr");
	}

	if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
	{
		return Comment->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	}

	return Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
}

bool FASCUtils::IsWidgetOfType(TSharedPtr<SWidget> Widget, const FString& WidgetTypeName, bool bCheckContains)
{
	if (!Widget.IsValid())
	{
		return false;
	}

	return bCheckContains ? Widget->GetTypeAsString().Contains(WidgetTypeName) : Widget->GetTypeAsString() == WidgetTypeName;
}

TSharedPtr<SWidget> FASCUtils::GetParentWidgetOfType(
	TSharedPtr<SWidget> Widget,
	const FString& ParentType)
{
	if (!Widget.IsValid())
	{
		return nullptr;
	}

	if (IsWidgetOfType(Widget, ParentType))
	{
		return Widget;
	}

	if (!Widget->IsParentValid())
	{
		return nullptr;
	}

	check(Widget->GetParentWidget() != Widget)

	TSharedPtr<SWidget> ReturnWidget = GetParentWidgetOfType(Widget->GetParentWidget(), ParentType);
	if (ReturnWidget.IsValid())
	{
		return ReturnWidget;
	}

	return nullptr;
}

void FASCUtils::ClearCommentNodes(UEdGraphNode_Comment* Comment, bool bUpdateCache)
{
	if (!Comment)
	{
		return;
	}

	Comment->ClearNodesUnderComment();

	if (bUpdateCache)
	{
		FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(Comment);
	}
}

bool FASCUtils::RemoveNodesFromComment(UEdGraphNode_Comment* Comment, const TSet<UObject*>& NodesToRemove, bool bUpdateCache)
{
	if (!Comment)
	{
		return false;
	}

	if (NodesToRemove.Num() == 0)
	{
		return false;
	}

	bool bDidRemoveAnything = false;
	const FCommentNodeSet NodesUnderComment = Comment->GetNodesUnderComment();

	// Clear all nodes under comment
	Comment->ClearNodesUnderComment();

	// Add back the nodes under comment while filtering out any which are to be removed
	for (UObject* NodeUnderComment : NodesUnderComment)
	{
		if (NodeUnderComment)
		{
			if (!NodesToRemove.Contains(NodeUnderComment))
			{
				AddNodeIntoComment(Comment, NodeUnderComment, false);
			}
			else
			{
				bDidRemoveAnything = true;
			}
		}
	}

	if (bUpdateCache)
	{
		FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(Comment);
	}

	return bDidRemoveAnything;
}

bool FASCUtils::AddNodeIntoComment(UEdGraphNode_Comment* Comment, UObject* NewNode, bool bUpdateCache)
{
	if (!Comment || !NewNode)
	{
		return false;
	}

	if (Comment->GetNodesUnderComment().Contains(NewNode))
	{
		return false;
	}

	Comment->AddNodeUnderComment(NewNode);

	if (bUpdateCache)
	{
		FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(Comment);
	}

	return true;
}

bool FASCUtils::AddNodesIntoComment(UEdGraphNode_Comment* Comment, const TSet<UObject*>& NewNodes, bool bUpdateCache)
{
	if (!Comment)
	{
		return false;
	}

	for (UObject* Node : NewNodes)
	{
		FASCUtils::AddNodeIntoComment(Comment, Node, false);
	}

	if (bUpdateCache)
	{
		FAutoSizeCommentsCacheFile::Get().UpdateNodesUnderComment(Comment);
	}

	return true;
}
