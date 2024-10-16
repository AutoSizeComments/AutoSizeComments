// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsGraphPanelNodeFactory.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsModule.h"
#include "AutoSizeCommentsSettings.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "Editor/Transactor.h"
#include "Framework/Commands/GenericCommands.h"
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "OtherGraphs/ASCComment_ControlRig.h"
#include "OtherGraphs/ASCComment_Material.h"

#if ASC_ENABLE_CONTROLRIG
#include "RigVMEditor/Public/Widgets/SRigVMGraphNodeComment.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#endif

TSharedPtr<SGraphNode> FAutoSizeCommentsGraphPanelNodeFactory::CreateNode(class UEdGraphNode* InNode) const
{
	if (!InNode)
	{
		return nullptr;
	}

	// check if the node was copy-pasted by checking if it was in the last transaction and it matches the undo description
	bool bWasCopyPasted = false;
	if (GEditor && GEditor->Trans)
	{
		const int Index = GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1;
		if (const FTransaction* LastTransaction = GEditor->Trans->GetTransaction(Index))
		{
			const FTransactionContext Context = LastTransaction->GetContext();

			if (LastTransaction->ContainsObject(InNode))
			{
				if (Context.Title.EqualTo(FGenericCommands::Get().Paste->GetDescription()))
				{
					bWasCopyPasted = true;
					// UE_LOG(LogTemp, Warning, TEXT("Last transaction Context %s Title %s"), *Context.Context, *Context.Title.ToString());
				}
			}
		}
	}

	// init graph handler for containing graph
	FAutoSizeCommentGraphHandler::Get().BindToGraph(InNode->GetGraph());

	const FASCGraphHandlerData& GraphData = FAutoSizeCommentGraphHandler::Get().GetGraphHandlerData(InNode->GetGraph());

	const UAutoSizeCommentsSettings& ASCSettings = UAutoSizeCommentsSettings::Get();

	if (ASCSettings.bDisableASCGraphNode)
	{
		return nullptr;
	}

	if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(InNode))
	{
		if (UEdGraph* Graph = InNode->GetGraph())
		{
			if (UClass* GraphClass = Graph->GetClass())
			{
				FString GraphClassName = GraphClass->GetName();

				if (ASCSettings.bDebugGraph_ASC)
				{
					UE_LOG(LogAutoSizeComments, Log, TEXT("GraphClassName: <%s>"), *GraphClassName);
				}

				TArray<FString> IgnoredClasses = ASCSettings.IgnoredGraphs;
				for (FString Ignored : IgnoredClasses)
				{
					if (GraphClassName == Ignored)
					{
						return nullptr;
					}
				}
			}

			TSharedPtr<SAutoSizeCommentsGraphNode> GraphNode;
			if (CommentNode->IsA(UMaterialGraphNode_Comment::StaticClass()))
			{
				GraphNode = SNew(SASCComment_Material, InNode);
			}
#if ASC_ENABLE_CONTROLRIG
			else if (CommentNode->GetSchema() && CommentNode->GetSchema()->IsA(URigVMEdGraphSchema::StaticClass()))
			{
				GraphNode = SNew(SASCComment_ControlRig, InNode);
			}
#endif
			else
			{
				GraphNode = SNew(SAutoSizeCommentsGraphNode, InNode);
			}

			if (GraphNode)
			{
				GraphNode->InitialSelection = GraphData.PreviousSelection;
				GraphNode->bWasCopyPasted = bWasCopyPasted;
				GraphNode->SlatePrepass();
				return GraphNode; 
			}
		}
	}

	return nullptr;
}
