#pragma once

class UEdGraphNode_Comment;

struct FASCUtils
{
	static TArray<UEdGraphNode_Comment*> GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments, UEdGraphNode* Node);
	static TArray<UEdGraphNode*> GetNodesUnderComment(UEdGraphNode_Comment* Comment);

	static TArray<UEdGraphPin*> GetPinsByDirection(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphPin*> GetLinkedPins(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphNode*> GetLinkedNodes(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static bool HasNodeBeenDeleted(UEdGraphNode* Node);
};
