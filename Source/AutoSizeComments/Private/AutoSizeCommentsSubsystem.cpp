// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsSubsystem.h"

#include "Editor.h"

UAutoSizeCommentsSubsystem& UAutoSizeCommentsSubsystem::Get()
{
	return *GEditor->GetEditorSubsystem<UAutoSizeCommentsSubsystem>();
}

void UAutoSizeCommentsSubsystem::MarkNodeDirty(UEdGraphNode_Comment* Node)
{
	DirtyComments.Add(Node);
}

bool UAutoSizeCommentsSubsystem::IsDirty(UEdGraphNode_Comment* Node)
{
	if (DirtyComments.Contains(Node))
	{
		DirtyComments.Remove(Node);
		return true;
	}

	return false;
}
