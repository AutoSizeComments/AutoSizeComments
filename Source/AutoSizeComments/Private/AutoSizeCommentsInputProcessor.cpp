// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsInputProcessor.h"

#include "AutoSizeCommentsCommands.h"
#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsUtils.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"

static TSharedPtr<FAutoSizeCommentsInputProcessor> ASCInputProcessor;

void FAutoSizeCommentsInputProcessor::Create()
{
	ASCInputProcessor = MakeShareable(new FAutoSizeCommentsInputProcessor());
	FSlateApplication::Get().RegisterInputPreProcessor(ASCInputProcessor);

	ASCInputProcessor->Init();
}

void FAutoSizeCommentsInputProcessor::Cleanup()
{
	if (ASCInputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(ASCInputProcessor);
		}

		ASCInputProcessor.Reset();
	}
}

void FAutoSizeCommentsInputProcessor::Init()
{
	ASCCommandList = FASCCommands::MakeCommandList();
}

FAutoSizeCommentsInputProcessor& FAutoSizeCommentsInputProcessor::Get()
{
	return *ASCInputProcessor;
}

bool FAutoSizeCommentsInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
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

bool FAutoSizeCommentsInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent)
{
	KeysDown.Add(KeyEvent.GetKey());

	if (FSlateApplication::Get().IsDragDropping())
	{
		return false;
	}

	FModifierKeysState ModifierKeysState = FSlateApplication::Get().GetModifierKeys();
	const FInputChord CheckChord(KeyEvent.GetKey(), EModifierKey::FromBools(
		ModifierKeysState.IsControlDown(),
		ModifierKeysState.IsAltDown(),
		ModifierKeysState.IsShiftDown(),
		ModifierKeysState.IsCommandDown()));

	TSharedPtr<FUICommandInfo> Command = FInputBindingManager::Get().FindCommandInContext(FASCCommands::Get().GetContextName(), CheckChord, false);
	if (Command.IsValid() && Command->HasActiveChord(CheckChord))
	{
		if (const FUIAction* Action = ASCCommandList->GetActionForCommand(Command))
		{
			if (Action->CanExecute() && (!KeyEvent.IsRepeat() || Action->CanRepeat()))
			{
				return Action->Execute();
			}
		}
	}

	return false;
}

bool FAutoSizeCommentsInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	KeysDown.Remove(InKeyEvent.GetKey());
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
