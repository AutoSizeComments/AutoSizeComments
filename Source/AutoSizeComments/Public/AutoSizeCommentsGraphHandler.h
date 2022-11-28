// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "AutoSizeCommentsMacros.h"
#include "AutoSizeCommentsNodeChangeData.h"

class UEdGraphNode_Comment;
class SGraphPanel;

struct FASCGraphHandlerData
{
	TSet<TWeakObjectPtr<UEdGraphNode_Comment>> RegisteredComments;
	TArray<TWeakObjectPtr<UEdGraphNode_Comment>> LastSelectionSet;
	FDelegateHandle OnGraphChangedHandle;

	TMap<TWeakObjectPtr<UEdGraphNode_Comment>, FASCCommentChangeData> CommentChangeData;
};

class FAutoSizeCommentGraphHandler
{
public:
	static FAutoSizeCommentGraphHandler& Get();
	static void TearDown();

	void BindDelegates();
	void UnbindDelegates();

	void BindToGraph(UEdGraph* Graph);

	void OnGraphChanged(const FEdGraphEditAction& Action);

	void AutoInsertIntoCommentNodes(TWeakObjectPtr<UEdGraphNode> Node, TWeakObjectPtr<UEdGraphNode> LastSelectedNode);

	void RegisterActiveGraphPanel(TSharedPtr<SGraphPanel> GraphPanel) { ActiveGraphPanels.Add(GraphPanel); }

	void RequestGraphVisualRefresh(TSharedPtr<SGraphPanel> GraphPanel);

	void ProcessAltReleased(TSharedPtr<SGraphPanel> GraphPanel);

	bool RegisterComment(UEdGraphNode_Comment* Comment);

	void UpdateCommentChangeState(UEdGraphNode_Comment* Comment);
	bool HasCommentChanged(UEdGraphNode_Comment* Comment);

private:
	TMap<TWeakObjectPtr<UEdGraph>, FASCGraphHandlerData> GraphDatas;

	TArray<TWeakPtr<SGraphPanel>> ActiveGraphPanels;

#if ASC_UE_VERSION_OR_LATER(5, 0)
	FTSTicker::FDelegateHandle TickDelegateHandle;
#else
	FDelegateHandle TickDelegateHandle;
#endif

	bool bPendingSave = false;

	bool bPendingGraphVisualRequest = false;

	bool bProcessedAltReleased = false;

	bool Tick(float DeltaTime);

	void UpdateNodeUnrelatedState();

	void OnNodeAdded(TWeakObjectPtr<UEdGraphNode> NewNodePtr);

	void OnNodeDeleted(const FEdGraphEditAction& Action);

#if ASC_UE_VERSION_OR_LATER(5, 0)
	void OnObjectPreSave(UObject* Object, FObjectPreSaveContext Context);
#endif

	void OnObjectSaved(UObject* Object);

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	void SaveSizeCache();

	void UpdateContainingComments(TWeakObjectPtr<UEdGraphNode> Node);

	void RefreshGraphVisualRefresh(TWeakPtr<SGraphPanel> GraphPanel);
};
