// Fill out your copyright notice in the Description page of Project Settings.

#include "AutoSizeCommentsInputProcessor.h"

#include "AutoSizeCommentsSettings.h"
#include "AutoSizeCommentsUtils.h"
#include "SGraphPanel.h"

static TSharedPtr<FAutoSizeCommentsInputProcessor> Instance;

void FAutoSizeCommentsInputProcessor::Create()
{
	Instance = MakeShareable(new FAutoSizeCommentsInputProcessor());
	FSlateApplication::Get().RegisterInputPreProcessor(Instance);
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

FAutoSizeCommentsInputProcessor& FAutoSizeCommentsInputProcessor::Get()
{
	return *Instance;
}

bool FAutoSizeCommentsInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (GetDefault<UAutoSizeCommentsSettings>()->bSelectNodeWhenClickingOnPin)
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

bool FAutoSizeCommentsInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	KeysDown.Add(InKeyEvent.GetKey());
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
