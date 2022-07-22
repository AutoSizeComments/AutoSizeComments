#pragma once

class UEdGraphNode_Comment;
class SGraphPin;
class SGraphPanel;

struct FASCUtils
{
	static TArray<UEdGraphNode_Comment*> GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments, UEdGraphNode* Node);
	static TArray<UEdGraphNode*> GetNodesUnderComment(UEdGraphNode_Comment* Comment);

	static TArray<UEdGraphPin*> GetPinsByDirection(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphPin*> GetLinkedPins(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphNode*> GetLinkedNodes(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static bool HasNodeBeenDeleted(UEdGraphNode* Node);

	static bool IsValidPin(UEdGraphPin* Pin);
	static TSharedPtr<SGraphPin> GetGraphPin(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin);
	static TSharedPtr<SGraphNode> GetGraphNode(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphNode* Node);

	static TSharedPtr<SGraphPin> GetHoveredGraphPin(TSharedPtr<SGraphPanel> GraphPanel);

	static TArray<UEdGraphNode_Comment*> GetSelectedComments(TSharedPtr<SGraphPanel> GraphPanel);

	static bool IsGraphReadOnly(TSharedPtr<SGraphPanel> GraphPanel);

	static FString GetNodeName(UEdGraphNode* Node);

	static bool IsWidgetOfType(TSharedPtr<SWidget> Widget, const FString& WidgetTypeName, bool bCheckContains = false);

	static TSharedPtr<SWidget> GetParentWidgetOfType(TSharedPtr<SWidget> Widget, const FString& ParentType);
};
