#include "AutoSizeCommentsState.h"

#include "AutoSizeCommentsGraphNode.h"
#include "EdGraphNode_Comment.h"

void FASCState::RegisterComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment)
{
	UEdGraphNode_Comment* Comment = ASCComment->GetCommentNodeObj();
	CommentToASCMapping.Add(Comment, ASCComment);
}

void FASCState::RemoveComment(UEdGraphNode_Comment* Comment)
{
	CommentToASCMapping.Remove(Comment);
}

TSharedPtr<SAutoSizeCommentsGraphNode> FASCState::GetASCComment(UEdGraphNode_Comment* Comment)
{
	TWeakPtr<SAutoSizeCommentsGraphNode> WeakPtr = CommentToASCMapping.FindRef(Comment);
	return WeakPtr.Pin();
}
