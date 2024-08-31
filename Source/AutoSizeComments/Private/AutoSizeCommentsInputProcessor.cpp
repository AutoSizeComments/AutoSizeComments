// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsInputProcessor.h"

#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsGraphNode.h"
#include "AutoSizeCommentsCommands.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsState.h"
#include "AutoSizeCommentsUtils.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"

static TSharedPtr<FAutoSizeCommentsInputProcessor> Instance;

void FAutoSizeCommentsInputProcessor::Create()
{
	Instance = MakeShareable(new FAutoSizeCommentsInputProcessor());
	FSlateApplication::Get().RegisterInputPreProcessor(Instance);

	Instance->Init();
}

void FAutoSizeCommentsInputProcessor::Cleanup()
{
	if (Instance.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(Instance);
		}

		Instance.Reset();
	}
}

void FAutoSizeCommentsInputProcessor::Init()
{
	ASCCommandList = FASCCommands::MakeCommandList();
}

FAutoSizeCommentsInputProcessor& FAutoSizeCommentsInputProcessor::Get()
{
	return *Instance;
}

bool FAutoSizeCommentsInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (OnKeyOrMouseDown(SlateApp, MouseEvent.GetEffectingButton()))
	{
		return true;
	}

	if (UAutoSizeCommentsSettings::Get().bSelectNodeWhenClickingOnPin)
	{
		// this logic is required for the auto insert comment to work correctly
		// we need to know where a node was dragged from, so select the node when you click the pin
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FWidgetPath WidgetPath = SlateApp.LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows());
			for (int i = WidgetPath.Widgets.Num() - 1; i >= 0; i--)
			{
				TSharedPtr<SWidget> Widget = WidgetPath.Widgets[i].Widget->AsShared();
				const FString WidgetName = Widget->GetTypeAsString();
				if (WidgetName == "SGraphPanel")
				{
					TSharedPtr<SGraphPanel> GraphPanel = StaticCastSharedPtr<SGraphPanel>(Widget);
					if (GraphPanel.IsValid())
					{
						TSharedPtr<SGraphPin> HoveredPin = FASCUtils::GetHoveredGraphPin(GraphPanel);
						if (HoveredPin.IsValid())
						{
							UEdGraphNode* OwningNode = HoveredPin->GetPinObj()->GetOwningNode();

							// select the owning node when it is not the only selected node 
							if (!GraphPanel->SelectionManager.IsNodeSelected(OwningNode) || GraphPanel->SelectionManager.GetSelectedNodes().Num() > 1)
							{
								GraphPanel->SelectionManager.SelectSingleNode(OwningNode);
							}
						}
					}

					break;
				}
			}
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (OnKeyOrMouseUp(SlateApp, MouseEvent.GetEffectingButton()))
	{
		return true;
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// ignore repeat keys
	if (InKeyEvent.IsRepeat())
	{
		return false;
	}

	if (OnKeyOrMouseDown(SlateApp, InKeyEvent.GetKey()))
	{
		return true;
	}

	KeysDown.Add(InKeyEvent.GetKey());

	if (FSlateApplication::Get().IsDragDropping())
	{
		return false;
	}

	FModifierKeysState ModifierKeysState = FSlateApplication::Get().GetModifierKeys();
	const FInputChord CheckChord(InKeyEvent.GetKey(), EModifierKey::FromBools(
		ModifierKeysState.IsControlDown(),
		ModifierKeysState.IsAltDown(),
		ModifierKeysState.IsShiftDown(),
		ModifierKeysState.IsCommandDown()));

	TSharedPtr<FUICommandInfo> Command = FInputBindingManager::Get().FindCommandInContext(FASCCommands::Get().GetContextName(), CheckChord, false);
	if (Command.IsValid() && Command->HasActiveChord(CheckChord))
	{
		if (const FUIAction* Action = ASCCommandList->GetActionForCommand(Command))
		{
			if (Action->CanExecute() && (!InKeyEvent.IsRepeat() || Action->CanRepeat()))
			{
				return Action->Execute();
			}
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (OnKeyOrMouseUp(SlateApp, InKeyEvent.GetKey()))
	{
		return true;
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::OnKeyOrMouseDown(FSlateApplication& SlateApp, const FKey& Key)
{
	KeysDown.Add(Key);

	if (UAutoSizeCommentsSettings::Get().RemoveNodeFromCommentKey.GetRelationship(UAutoSizeCommentsSettings::Get().AddNodeToCommentKey) == FInputChord::ERelationshipType::Masked)
	{
		if (RunRemoveNodeCommand(Key))
		{
			return true;
		}

		if (RunAddNodeCommand(Key))
		{
			return true;
		}
	}
	else
	{
		if (RunAddNodeCommand(Key))
		{
			return true;
		}

		if (RunRemoveNodeCommand(Key))
		{
			return true;
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::OnKeyOrMouseUp(FSlateApplication& SlateApp, const FKey& Key)
{
	KeysDown.Remove(Key);

	// should call this after removing from keys down so we can check if the other key is still down
	if (Key == EKeys::LeftMouseButton || Key == EKeys::LeftAlt)
	{
		if (KeysDown.Contains(EKeys::LeftMouseButton) || KeysDown.Contains(EKeys::LeftAlt))
		{
			// check alt dragging method
			if (TSharedPtr<SGraphPanel> GraphPanel = FASCUtils::GetHoveredGraphPanel())
			{
				TSet<UEdGraphNode*> SelectedNodes = FASCUtils::GetSelectedNodes(GraphPanel, false);
				if (SelectedNodes.Num() > 0)
				{
					FAutoSizeCommentGraphHandler::Get().ProcessAltReleased(GraphPanel);
				}
			}
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::IsKeyDown(const FKey& Key)
{
	return KeysDown.Contains(Key);
}

bool FAutoSizeCommentsInputProcessor::IsInputChordDown(const FInputChord& Chord)
{
	const FModifierKeysState ModKeysState = FSlateApplication::Get().GetModifierKeys();
	const bool AreModifiersDown = ModKeysState.AreModifersDown(EModifierKey::FromBools(Chord.bCtrl, Chord.bAlt, Chord.bShift, Chord.bCmd));
	return KeysDown.Contains(Chord.Key) && AreModifiersDown;
}

bool FAutoSizeCommentsInputProcessor::RunAddNodeCommand(const FKey& Key)
{
	if (IsInputChordDown(UAutoSizeCommentsSettings::Get().AddNodeToCommentKey))
	{
		if (TSharedPtr<SGraphPanel> GraphPanel = FASCUtils::GetHoveredGraphPanel())
		{
			if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(FASCUtils::GetSelectedNode(GraphPanel)))
			{
				if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(SelectedComment))
				{
					if (TSharedPtr<SGraphNode> HoveredGraphNode = FASCUtils::GetHoveredGraphNode(GraphPanel))
					{
						if (!ASCComment->HasNode(HoveredGraphNode->GetNodeObj()))
						{
							const FScopedTransaction Transaction(INVTEXT("Add node into comment"));
							if (ASCComment->AddNode(HoveredGraphNode->GetNodeObj()))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::RunRemoveNodeCommand(const FKey& Key)
{
	if (IsInputChordDown(UAutoSizeCommentsSettings::Get().RemoveNodeFromCommentKey))
	{
		if (TSharedPtr<SGraphPanel> GraphPanel = FASCUtils::GetHoveredGraphPanel())
		{
			if (UEdGraphNode_Comment* SelectedComment = Cast<UEdGraphNode_Comment>(FASCUtils::GetSelectedNode(GraphPanel)))
			{
				if (TSharedPtr<SAutoSizeCommentsGraphNode> ASCComment = FASCState::Get().GetASCComment(SelectedComment))
				{
					if (TSharedPtr<SGraphNode> HoveredGraphNode = FASCUtils::GetHoveredGraphNode(GraphPanel))
					{
						if (ASCComment->HasNode(HoveredGraphNode->GetNodeObj()))
						{
							const FScopedTransaction Transaction(INVTEXT("Remove node from comment"));
							ASCComment->RemoveNode(HoveredGraphNode->GetNodeObj());
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}
