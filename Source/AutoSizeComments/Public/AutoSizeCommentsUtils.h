#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h" // EGraphType, EEdGraphPinDirection

class SAutoSizeCommentsGraphNode;
class UEdGraphNode;
class UEdGraphPin;
class UEdGraphNode_Comment;
class SGraphPin;
class SGraphPanel;

struct FASCUtils
{
	static TArray<UEdGraphNode_Comment*> GetContainingCommentNodes(const TArray<UEdGraphNode_Comment*>& Comments, UEdGraphNode* Node);
	static TArray<UEdGraphNode*> GetNodesUnderComment(UEdGraphNode_Comment* Comment);

	static TArray<UEdGraphPin*> GetPinsByDirection(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphPin*> GetLinkedPins(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);

	static TArray<UEdGraphNode*> GetLinkedNodes(const UEdGraphNode* Node, EEdGraphPinDirection Direction = EGPD_MAX);
	static TArray<UEdGraphNode_Comment*> GetCommentsFromGraph(UEdGraph* Graph);
	static TArray<TSharedPtr<SAutoSizeCommentsGraphNode>> GetASCCommentsFromGraph(UEdGraph* Graph);

	static bool HasNodeBeenDeleted(UEdGraphNode* Node);

	static bool IsValidPin(UEdGraphPin* Pin);

	static TSharedPtr<SWidget> GetChildWidget(TSharedPtr<SWidget> Widget, const FName& WidgetClassName);

	static TSharedPtr<SGraphPin> GetGraphPin(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphPin* Pin);
	static TSharedPtr<SGraphNode> GetGraphNode(TSharedPtr<SGraphPanel> GraphPanel, UEdGraphNode* Node);

	static TSharedPtr<SGraphPin> GetHoveredGraphPin(TSharedPtr<SGraphPanel> GraphPanel);
	static TSharedPtr<SGraphNode> GetHoveredGraphNode(TSharedPtr<SGraphPanel> GraphPanel);
	static TSharedPtr<SGraphPanel> GetHoveredGraphPanel();

	static TArray<UEdGraphNode_Comment*> GetSelectedComments(TSharedPtr<SGraphPanel> GraphPanel);
	static TSet<UEdGraphNode*> GetSelectedNodes(TSharedPtr<SGraphPanel> GraphPanel, bool bExpandComments);
	static UEdGraphNode* GetSelectedNode(TSharedPtr<SGraphPanel> GraphPanel, bool bExpandComments = false);

	static bool IsGraphReadOnly(TSharedPtr<SGraphPanel> GraphPanel);

	static FString GetNodeName(UEdGraphNode* Node);

	static bool IsWidgetOfType(TSharedPtr<SWidget> Widget, const FString& WidgetTypeName, bool bCheckContains = false);

	static TSharedPtr<SWidget> GetParentWidgetOfType(TSharedPtr<SWidget> Widget, const FString& ParentType);

	static TSharedPtr<SWidget> GetParentWidgetOfTypes(TSharedPtr<SWidget> Widget, const TArray<FString>& ParentTypes);

	static bool DoesCommentContainComment(UEdGraphNode_Comment* Source, UEdGraphNode_Comment* Other);

	static void SetCommentFontSizeAndColor(UEdGraphNode_Comment* Comment, int32 FontSize, const FLinearColor& Color, bool bModify = true);

	static void ModifyObject(UObject* Obj);

	static UPackage* GetPackage(UObject* Obj);

	static bool HasReliableGuid(UEdGraph* Graph);

	static bool IsMajorNode(UEdGraphNode* Node);
};
