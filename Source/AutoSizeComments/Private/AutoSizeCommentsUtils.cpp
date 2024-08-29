#include "AutoSizeCommentsUtils.h"

#include "AutoSizeCommentNodeState.h"
#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsState.h"
#include "EdGraphNode_Comment.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"

TArray<UEdGraphNode_Comment*> FASCUtils::GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments, UEdGraphNode* Node)
{
	TArray<UEdGraphNode_Comment*> ContainingComments;
	for (auto Comment : Comments)
	{
		if (Comment)
		{
			const TSet<UEdGraphNode*>& NodesUnderComments = UASCNodeState::Get(Comment)->GetNodesUnderComment();
			if (NodesUnderComments.Contains(Node))
			{
				ContainingComments.Add(Comment);
			}
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

TArray<UEdGraphNode_Comment*> FASCUtils::GetCommentsFromGraph(UEdGraph* Graph)
{
	TArray<UEdGraphNode_Comment*> Comments;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
		{
			Comments.Add(Comment);
		}
	}

	return Comments;
}

TArray<TSharedPtr<SAutoSizeCommentsGraphNode>> FASCUtils::GetASCCommentsFromGraph(UEdGraph* Graph)
{
	TArray<TSharedPtr<SAutoSizeCommentsGraphNode>> OutASCComments;
	for (UEdGraphNode_Comment* Comment : GetCommentsFromGraph(Graph))
	{
		if (auto ASCComment = FASCState::Get().GetASCComment(Comment))
		{
			OutASCComments.Add(ASCComment);
		}
	}

	return OutASCComments;
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

TSharedPtr<SWidget> FASCUtils::GetChildWidget(TSharedPtr<SWidget> Widget, const FName& WidgetClassName)
{
	if (Widget.IsValid())
	{
		if (Widget->GetType() == WidgetClassName)
		{
			return Widget;
		}

		// iterate through children
		if (FChildren* Children = Widget->GetChildren())
		{
			for (int i = 0; i < Children->Num(); i++)
			{
				TSharedPtr<SWidget> ReturnWidget = GetChildWidget(Children->GetChildAt(i), WidgetClassName);
				if (ReturnWidget.IsValid())
				{
					return ReturnWidget;
				}
			}
		}
	}

	return nullptr;
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

TSharedPtr<SGraphNode> FASCUtils::GetHoveredGraphNode(TSharedPtr<SGraphPanel> GraphPanel)
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
		if (auto GraphNode = GetGraphNode(GraphPanel, Node))
		{
			if (GraphNode->IsHovered())
			{
				return GraphNode;
			}
		}
	}

	return nullptr;
}

TSharedPtr<SGraphPanel> FASCUtils::GetHoveredGraphPanel()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	FWidgetPath Path = SlateApp.LocateWindowUnderMouse(SlateApp.GetCursorPos(), SlateApp.GetInteractiveTopLevelWindows());
	if (Path.IsValid())
	{
		for (int32 PathIndex = Path.Widgets.Num() - 1; PathIndex >= 0; PathIndex--)
		{
			TSharedRef<SWidget> Widget = Path.Widgets[PathIndex].Widget;
			TSharedPtr<SWidget> GraphPanelAsWidget = GetChildWidget(Widget, "SGraphPanel");
			if (GraphPanelAsWidget.IsValid())
			{
				auto GraphPanel = StaticCastSharedPtr<SGraphPanel>(GraphPanelAsWidget);
				if (GraphPanel.IsValid())
				{
					return GraphPanel;
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

TSet<UEdGraphNode*> FASCUtils::GetSelectedNodes(TSharedPtr<SGraphPanel> GraphPanel, bool bExpandComments)
{
	TSet<UEdGraphNode*> SelectedNodes;
	if (!GraphPanel)
	{
		return SelectedNodes;
	}

	for (UObject* Obj : GraphPanel->SelectionManager.GetSelectedNodes())
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(Obj))
		{
			SelectedNodes.Add(Node);

			if (bExpandComments)
			{
				if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(Node))
				{
					SelectedNodes.Append(UASCNodeState::Get(SelectedComment)->GetNodesUnderComment());
				}
			}
		}
	}

	return SelectedNodes;
}

UEdGraphNode* FASCUtils::GetSelectedNode(TSharedPtr<SGraphPanel> GraphPanel, bool bExpandComments)
{
	TSet<UEdGraphNode*> SelectedNodes = GetSelectedNodes(GraphPanel, bExpandComments);
	if (SelectedNodes.Num() == 1)
	{
		return SelectedNodes.Get(FSetElementId::FromInteger(0));
	}

	return nullptr;
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

TSharedPtr<SWidget> FASCUtils::GetParentWidgetOfTypes(TSharedPtr<SWidget> Widget, const TArray<FString>& ParentTypes)
{
	if (!Widget.IsValid())
	{
		return nullptr;
	}

	for (const FString& Type : ParentTypes)
	{
		if (IsWidgetOfType(Widget, Type))
		{
			return Widget;
		}
	}

	if (!Widget->IsParentValid())
	{
		return nullptr;
	}

	TSharedPtr<SWidget> ReturnWidget = GetParentWidgetOfTypes(Widget->GetParentWidget(), ParentTypes);
	if (ReturnWidget.IsValid())
	{
		return ReturnWidget;
	}

	return nullptr;
}

bool FASCUtils::DoesCommentContainComment(UEdGraphNode_Comment* Source, UEdGraphNode_Comment* Other)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FASCUtils::DoesCommentContainComment"), STAT_ASC_DoesCommentContainComment, STATGROUP_AutoSizeComments);
	struct FLocal
	{
		static bool DoesCommentContainComment_Recursive(UEdGraphNode_Comment* Source, UEdGraphNode_Comment* Other, TSet<UEdGraphNode*>& Visited)
		{
			if (Visited.Contains(Source))
			{
				return false;
			}

			Visited.Add(Source);


			for (UObject* Node : Source->GetNodesUnderComment())
			{
				if (Node == Other)
				{
					return true;
				}

				if (UEdGraphNode_Comment* NextComment = Cast<UEdGraphNode_Comment>(Node))
				{
					if (DoesCommentContainComment_Recursive(NextComment, Other, Visited))
					{
						return true;
					}
				}
			}

			return false;
		}
	};

	TSet<UEdGraphNode*> Visited;
	return FLocal::DoesCommentContainComment_Recursive(Source, Other, Visited);
}

void FASCUtils::SetCommentFontSizeAndColor(UEdGraphNode_Comment* Comment, int32 FontSize, const FLinearColor& Color, bool bModify)
{
	if (Comment)
	{
		if (bModify)
		{
			Comment->Modify();
		}

		Comment->FontSize = FontSize;
		Comment->CommentColor = Color;
	}
}
