// Copyright 2021 fpwong. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsMacros.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ResizingMode = EASCResizingMode::Always;
	AutoInsertComment = EASCAutoInsertComment::Always;
	bSelectNodeWhenClickingOnPin = true;
	CommentNodePadding = FVector2D(30, 30);
	MinimumVerticalPadding = 24.0f;
	CommentTextPadding = FMargin(2, 0, 2, 0);
	CommentTextAlignment = ETextJustify::Left;
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	bUseRandomColor = true;
	RandomColorOpacity = 1.f;
	bUseRandomColorFromList = false;
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 0, 1));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 1));
	MinimumControlOpacity = 0.f;
	DefaultCommentColor = FLinearColor::White;
	HeaderStyle.Color = FLinearColor::Gray;
	bAggressivelyUseDefaultColor = false;
	bMoveEmptyCommentBoxes = false;
	EmptyCommentBoxSpeed = 10;
	bHideCommentBubble = false;
	bEnableCommentBubbleDefaults = false;
	bDefaultColorCommentBubble = false;
	bDefaultShowBubbleWhenZoomed = true;
	CacheSaveLocation = EASCCacheSaveLocation::Plugin;
	bSaveCommentNodeDataToFile = true;
	bSaveCommentDataOnSavingGraph = true;
	bSaveCommentDataOnExit = true;
	bDetectNodesContainedForNewComments = true;
	ResizeChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	ResizeCollisionMethod = ECommentCollisionMethod::Contained;
	EnableCommentControlsKey = FInputChord();
	AltCollisionMethod = ECommentCollisionMethod::Intersect;
	bSnapToGridWhileResizing = false;
	bIgnoreKnotNodes = false;
	bIgnoreKnotNodesWhenPressingAlt = false;
	bIgnoreKnotNodesWhenResizing = false;
	bIgnoreSelectedNodesOnCreation = false;
	bRefreshContainingNodesOnMove = false;
	bDisableTooltip = false;
	bHighlightContainingNodesOnSelection = true;
#if !(ASC_UE_VERSION_OR_LATER(5, 0))
	IgnoredGraphs.Add("ControlRigGraph");
#endif
	bSuppressSuggestedSettings = false;
	bSuppressSourceControlNotification = false;
	bHideResizeButton = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCornerPoints = false;

	bEnableFixForSortDepthIssue = false;

	bDebugGraph_ASC = false;
	bDisablePackageCleanup = false;
}

TSharedRef<IDetailCustomization> FASCSettingsDetails::MakeInstance()
{
	return MakeShareable(new FASCSettingsDetails);
}
void FASCSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("CommentCache");
	auto& SizeCache = FAutoSizeCommentsCacheFile::Get();

	const FString CachePath = SizeCache.GetCachePath(true);

	const auto DeleteSizeCache = [&SizeCache]()
	{
		static FText Title = FText::FromString("Clear comment cache");
		static FText Message = FText::FromString("Are you sure you want to delete the comment cache?");

		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);
		if (Result == EAppReturnType::Yes)
		{
			SizeCache.DeleteCache();
		}

		return FReply::Handled();
	};

	GeneralCategory.AddCustomRow(FText::FromString("Clear comment cache"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Clear comment cache"))
			.Font(ASC_GET_FONT_STYLE(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(5).AutoWidth()
			[
				SNew(SButton)
				.Text(FText::FromString("Clear comment cache"))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Delete comment cache file located at: %s"), *CachePath)))
				.OnClicked_Lambda(DeleteSizeCache)
			]
		];
	
}
