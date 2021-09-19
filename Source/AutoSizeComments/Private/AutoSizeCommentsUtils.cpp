#include "AutoSizeCommentsUtils.h"

#include "EdGraphNode_Comment.h"

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
