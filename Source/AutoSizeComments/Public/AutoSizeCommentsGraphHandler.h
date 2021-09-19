#pragma once

class FAutoSizeCommentGraphHandler
{
public:
	void BindDelegates();
	void UnbindDelegates();

	void BindToGraph(UEdGraph* Graph);

	void OnGraphChanged(const FEdGraphEditAction& Action);

	void AutoInsertIntoCommentNodes(UEdGraphNode* Node);

private:
	TMap<TWeakObjectPtr<UEdGraph>, FDelegateHandle> GraphHandles;

	void OnObjectSaved(UObject* Object);

	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	void SaveSizeCache();

	void UpdateContainingComments(UEdGraphNode* Node);

	bool bPendingSave = false;
};
