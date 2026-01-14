// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphNode_Comment.h"
#include "EditorSubsystem.h"
#include "AutoSizeCommentsSubsystem.generated.h"

UCLASS()
class AUTOSIZECOMMENTS_API UAutoSizeCommentsSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static UAutoSizeCommentsSubsystem& Get();

	/* This is externally called from BlueprintAssist plugin to update comment nodes */
	UFUNCTION()
	void MarkNodeDirty(UEdGraphNode_Comment* Node);

	TSet<TWeakObjectPtr<UEdGraphNode_Comment>> DirtyComments;

	bool IsDirty(UEdGraphNode_Comment* Node);
};
