#pragma once

class UEdGraphNode_Comment;
class SAutoSizeCommentsGraphNode;

struct FASCState
{
	static FASCState& Get();
	static void TearDown();

	FASCState() = default;

	TMap<FGuid, TWeakPtr<SAutoSizeCommentsGraphNode>> CommentToASCMapping;

	void RegisterComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment);
	void RemoveComment(const UEdGraphNode_Comment* Comment);

	TSharedPtr<SAutoSizeCommentsGraphNode> GetASCComment(const UEdGraphNode_Comment* Comment);
	bool HasRegisteredComment(UEdGraphNode_Comment* Comment);
};
