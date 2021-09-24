#pragma once

class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;

struct FASCState
{
	FASCState() = default;

	TMap<FGuid, TWeakPtr<SAutoSizeCommentsGraphNode>> CommentToASCMapping;

	void RegisterComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment);
	void RemoveComment(UEdGraphNode_Comment* Comment);

	TSharedPtr<SAutoSizeCommentsGraphNode> GetASCComment(UEdGraphNode_Comment* Comment);
	bool HasRegisteredComment(UEdGraphNode_Comment* Comment);
};
