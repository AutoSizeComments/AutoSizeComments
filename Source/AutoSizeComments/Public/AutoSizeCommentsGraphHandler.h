// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsMacros.h"
#include "AutoSizeCommentsNodeChangeData.h"

enum class EASCResizingMode : uint8;
class UEdGraphNode_Comment;
class SGraphPanel;

struct FASCGraphHandlerData
{
	TArray<TWeakObjectPtr<UEdGraphNode_Comment>> LastSelectionSet;
	FDelegateHandle OnGraphChangedHandle;

	TMap<FGuid, FASCCommentChangeData> CommentChangeData;
	FASCGraphData GraphCacheData;

	float LastZoomLevel = -1;
	EGraphRenderingLOD::Type LastLOD = EGraphRenderingLOD::Type::DefaultDetail;
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

	void RegisterActiveGraphPanel(TSharedPtr<SGraphPanel> GraphPanel);

	void RequestGraphVisualRefresh(TSharedPtr<SGraphPanel> GraphPanel);

	void ProcessAltReleased(TSharedPtr<SGraphPanel> GraphPanel);

	FASCGraphHandlerData& GetGraphHandlerData(UEdGraph* Graph) { return GraphDatas.FindOrAdd(Graph); }
	void UpdateCommentChangeState(UEdGraphNode_Comment* Comment);
	bool HasCommentChangeState(UEdGraphNode_Comment* Comment) const;
	bool HasCommentChanged(UEdGraphNode_Comment* Comment);

	TArray<UEdGraph*> GetActiveGraphs();

	EGraphRenderingLOD::Type GetGraphLOD(TSharedPtr<SGraphPanel> GraphPanel);

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

	EASCResizingMode GetResizingMode(UEdGraph* Graph) const;

	void CheckCacheDataError(UEdGraph* Graph);
};
