#pragma once

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

private:
	TMap<TWeakObjectPtr<UEdGraph>, FDelegateHandle> GraphHandles;

	void OnNodeAdded(const FEdGraphEditAction& Action);

	void OnNodeDeleted(const FEdGraphEditAction& Action);

	void OnObjectSaved(UObject* Object);

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	void SaveSizeCache();

	void UpdateContainingComments(TWeakObjectPtr<UEdGraphNode> Node);

	bool bPendingSave = false;
};
