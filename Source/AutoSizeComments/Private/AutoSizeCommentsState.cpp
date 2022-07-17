#include "AutoSizeCommentsState.h"

#include "AutoSizeCommentsGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Misc/LazySingleton.h"

FASCState& FASCState::Get()
{
	return TLazySingleton<FASCState>::Get();
}

void FASCState::TearDown()
{
	TLazySingleton<FASCState>::TearDown();
}

void FASCState::RegisterComment(TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment)
{
	UEdGraphNode_Comment* Comment = ASCComment->GetCommentNodeObj();
	CommentToASCMapping.Add(Comment->NodeGuid, ASCComment);
}

void FASCState::RemoveComment(const UEdGraphNode_Comment* Comment)
{
	CommentToASCMapping.Remove(Comment->NodeGuid);
}
	
TSharedPtr<SAutoSizeCommentsGraphNode> FASCState::GetASCComment(const UEdGraphNode_Comment* Comment)
{
	if (!Comment)
	{
		return nullptr;
	}

	TWeakPtr<SAutoSizeCommentsGraphNode> WeakPtr = CommentToASCMapping.FindRef(Comment->NodeGuid);
	if (WeakPtr.IsValid())
	{
		return WeakPtr.Pin();
	}

	return nullptr;
}

bool FASCState::HasRegisteredComment(UEdGraphNode_Comment* Comment)
{
	return CommentToASCMapping.Contains(Comment->NodeGuid);
}
