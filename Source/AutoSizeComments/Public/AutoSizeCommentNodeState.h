// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Object.h"
#include "AutoSizeCommentNodeState.generated.h"

class UEdGraphNode;
class UEdGraphNode_Comment;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCNodeStateChanged, class UASCNodeState*);


/* The central state of the node for which other classes follow */
UCLASS(Transient)
class AUTOSIZECOMMENTS_API UASCNodeState : public UObject
{
	GENERATED_BODY()

public:
	UASCNodeState();

	static UASCNodeState* Get(UEdGraphNode_Comment* Comment);

	void Initialize(UEdGraphNode_Comment* Comment);
	void Cleanup();

	UPROPERTY()
	TWeakObjectPtr<UEdGraphNode_Comment> CommentNode;

	virtual void PostEditUndo() override;

	void InitializeFromCache();
	bool WriteNodesToComment();
	bool WriteHeaderToComment();
	bool SyncStateToComment();
	void UpdateCommentStateChange(bool bUpdateParentComments = true);

	bool CanAddNode(UEdGraphNode* Node, bool bIgnoreKnots = false);

	bool AddNode(UEdGraphNode* Node, bool bUpdateComment = true);
	bool RemoveNode(UEdGraphNode* Node, bool bUpdateComment = true);
	bool AddNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment = true);
	bool RemoveNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment = true);
	void ClearNodes(bool bUpdateComment = true);
	bool ReplaceNodes(const TArray<UEdGraphNode*>& Nodes, bool bUpdateComment = true);

	const TSet<UEdGraphNode*>& GetNodesUnderComment() { return NodesUnderComment; }
	TSet<UEdGraphNode*>& GetNodesUnderCommentMut() { return NodesUnderComment; }
	TSet<UEdGraphNode*> GetMajorNodesUnderComment();

	void AddChild(UASCNodeState* ChildNodeState);
	void RemoveChild(UASCNodeState* ChildNodeState);

	void UpdateParentComments();

	bool SetIsHeader(bool bNewValue);
	bool IsHeader() const { return bIsHeader; }

	void SetStyle(UEdGraphNode_Comment* Comment);

protected:
	UPROPERTY()
	TSet<UEdGraphNode*> NodesUnderComment;

	UPROPERTY()
	TArray<UASCNodeState*> ParentComments;

	UPROPERTY()
	TArray<UASCNodeState*> ChildComments;

	UPROPERTY()
	bool bIsHeader = false;

	// required to check in PostEditUndo
	bool bPrevIsHeader = false;
};

class AUTOSIZECOMMENTS_API FASCNodeStateManager
{
public:
	static FASCNodeStateManager& Get();
	static void TearDown();

	bool Init();
	void Cleanup();

	void CleanupCommentStateMap();

	void CleanupOnAssetClosed(UObject* Asset, EAssetEditorCloseReason CloseReason);

	TMap<FGuid, UASCNodeState*> CommentStateMap;
	UASCNodeState* GetCommentState(UEdGraphNode_Comment* Comment);

protected:
	bool bInitialized = false;
};