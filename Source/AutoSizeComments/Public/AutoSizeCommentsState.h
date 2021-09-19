#pragma once

class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;

struct FASCState
{
	FASCState() = default;

	TMap<UEdGraphNode_Comment*, TWeakPtr<SAutoSizeCommentsGraphNode>> CommentToASCMapping;

	void RegisterComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment);
	void RemoveComment(UEdGraphNode_Comment* Comment);

	TSharedPtr<SAutoSizeCommentsGraphNode> GetASCComment(UEdGraphNode_Comment* Comment);
};
