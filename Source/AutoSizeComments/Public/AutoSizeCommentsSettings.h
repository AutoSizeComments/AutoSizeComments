// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

#include "Framework/Text/TextLayout.h"
#include "Layout/Margin.h"

#include "AutoSizeCommentsSettings.generated.h"

UENUM()
enum class EASCCacheSaveMethod : uint8
{
	/** Save the cache to an external json file */
	File UMETA(DisplayName = "File"),

	/** Save to cache in the package's meta data (the .uasset) */
	MetaData UMETA(DisplayName = "Package Meta Data"),
};

UENUM()
enum class EASCCacheSaveLocation : uint8
{
	/** Save to PluginFolder/ASCCache/PROJECT_ID.json */
	Plugin UMETA(DisplayName = "Plugin"),

	/** Save to ProjectFolder/Saved/AutoSizeComments/AutoSizeCommentsCache.json */
	Project UMETA(DisplayName = "Project"),
};

UENUM()
enum class EASCResizingMode : uint8
{
	/** Resize to containing nodes on tick */
	Always UMETA(DisplayName = "Always"),

	/** Resize when we detect a containing node moves or changes size */
	Reactive UMETA(DisplayName = "Reactive"),

	/** Never resize */
	Disabled UMETA(DisplayName = "Disabled"),
};

UENUM()
enum class ECommentCollisionMethod : uint8
{
	/** Add the node if the top-left corner is inside the comment */
	Point UMETA(DisplayName = "Point"),

	/** Add the node if it is intersecting the comment */
	Intersect UMETA(DisplayName = "Overlap"),

	/** Add the node if it is fully contained in the comment */
	Contained UMETA(DisplayName = "Contained"),
	Disabled UMETA(DisplayName = "Disabled"),
};

UENUM()
enum class EASCAutoInsertComment : uint8
{
	/** Never insert new nodes into comments */
	Never UMETA(DisplayName = "Never"),

	/** Insert new nodes when a node is created from a pin */
	Always UMETA(DisplayName = "Always"),

	/** Insert new nodes when a node is created from a pin (and is surrounded by multiple nodes inside the comment) */
	Surrounded UMETA(DisplayName = "Surrounded"),
};

UENUM()
enum class EASCDefaultCommentColorMethod : uint8
{
	/** Use the default engine comment color */
	None UMETA(DisplayName = "None"),

	/** Use a random color when spawning the comment */
	Random UMETA(DisplayName = "Random"),

	/** Use the plugin color `DefaultCommentColor` when spawning the comment */
	Default UMETA(DisplayName = "Default"),
};

USTRUCT()
struct FPresetCommentStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, config, Category = Default)
	int FontSize = 18;

	UPROPERTY(EditAnywhere, config, Category = Default)
	bool bSetHeader = false;
};

USTRUCT()
struct FASCGraphSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = Default)
	EASCResizingMode ResizingMode = EASCResizingMode::Always;
};

UCLASS(config = EditorPerProjectUserSettings)
class AUTOSIZECOMMENTS_API UAutoSizeCommentsSettings final : public UObject
{
	GENERATED_BODY()

public:
	UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer);

	/** The default font size for comment boxes */
	UPROPERTY(EditAnywhere, config, Category = UI)
	int DefaultFontSize;

	/** If enabled, all nodes will be changed to the default font size (unless they are a preset or floating node) */
	UPROPERTY(EditAnywhere, config, Category = UI)
	bool bUseDefaultFontSize;

	UPROPERTY(EditAnywhere, Config, Category = Color)
	EASCDefaultCommentColorMethod DefaultCommentColorMethod;

	/** If Use Random Color is not enabled, comment boxes will spawn with this default color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Default", EditConditionHides))
	FLinearColor DefaultCommentColor;

	/** Set all comments on the graph to the default color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Default", EditConditionHides))
	bool bAggressivelyUseDefaultColor;

	/** Opacity used for the random color */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides))
	float RandomColorOpacity;

	/** If enabled, select a random color from predefined list */
	UPROPERTY(EditAnywhere, config, Category = Color, meta=(EditCondition="DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides))
	bool bUseRandomColorFromList;

	/** If UseRandomColorFromList is enabled, new comments will select a color from one of these */
	UPROPERTY(EditAnywhere, config, Category = Color, meta = (EditCondition = "bUseRandomColorFromList && DefaultCommentColorMethod==EASCDefaultCommentColorMethod::Random", EditConditionHides))
	TArray<FLinearColor> PredefinedRandomColorList;

	/** Minimum opacity for comment box controls when not hovered */
	UPROPERTY(EditAnywhere, config, Category = Color)
	float MinimumControlOpacity;

	/** Style for header comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Styles)
	FPresetCommentStyle HeaderStyle;

	/** Preset styles (each style will have its own button on the comment box) */
	UPROPERTY(EditAnywhere, config, Category = Styles)
	TArray<FPresetCommentStyle> PresetStyles;

	/** Preset style that will apply if the title starts with the according prefix */
	UPROPERTY(EditAnywhere, config, Category = Styles)
	TMap<FString, FPresetCommentStyle> TaggedPresets;

	/** The title bar uses a minimal style when being edited (requires UE5 or later) */
	UPROPERTY(EditAnywhere, config, Category = "UI")
	bool bUseMinimalTitlebarStyle = false;

	/** Always hide the comment bubble */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble)
	bool bHideCommentBubble;

	/** Set default values for comment bubble */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble)
	bool bEnableCommentBubbleDefaults;

	/** Default value for "Color Bubble" */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (EditCondition = "bEnableCommentBubbleDefaults"))
	bool bDefaultColorCommentBubble;

	/** Default value for "Show Bubble When Zoomed" */
	UPROPERTY(EditAnywhere, config, Category = CommentBubble, meta = (EditCondition = "bEnableCommentBubbleDefaults"))
	bool bDefaultShowBubbleWhenZoomed;

	/** The auto resizing behavior for comments (always: on tick | reactive: upon detecting node movement) */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	EASCResizingMode ResizingMode;

	/** Determines when to insert newly created nodes into existing comments */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	EASCAutoInsertComment AutoInsertComment;

	/** When a user places a comment, give keyboard focus to the title bar so it can be easily renamed */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bAutoRenameNewComments;

	/** When you click a node's pin, also select the node (required for AutoInsertComment to function correctly) */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bSelectNodeWhenClickingOnPin;

	/** Amount of padding for around the contents of a comment node */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	FVector2D CommentNodePadding;

	/** Amount of padding around the comment title text */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	FMargin CommentTextPadding;

	/** Minimum vertical padding above and below the node */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay)
	float MinimumVerticalPadding;

	/** Comment text alignment */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	TEnumAsByte<ETextJustify::Type> CommentTextAlignment;

	/** If enabled, add any containing node's comment bubble to the comment bounds */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bUseCommentBubbleBounds;

	/** If enabled, empty comment boxes will move out of the way of other comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bMoveEmptyCommentBoxes;

	/** The speed at which empty comment boxes move */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	float EmptyCommentBoxSpeed;

	/** Choose cache save method: as an external file or inside the package's metadata */
	UPROPERTY(EditAnywhere, config, Category = CommentCache)
	EASCCacheSaveMethod CacheSaveMethod;

	/** Choose where to save the json file: project or plugin folder */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (EditCondition = "CacheSaveMethod == EASCCacheSaveMethod::File", EditConditionHides))
	EASCCacheSaveLocation CacheSaveLocation;

	/** If enabled, nodes will be saved to file when the graph is saved */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (EditCondition = "bSaveCommentNodeDataToFile"))
	bool bSaveCommentDataOnSavingGraph;

	/** If enabled, nodes will be saved to file when the program is exited */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, meta = (EditCondition = "bSaveCommentNodeDataToFile"))
	bool bSaveCommentDataOnExit;

	/** If enabled, cache file JSON text will be made more human-readable, but increases file size */
	UPROPERTY(EditAnywhere, config, Category = CommentCache, AdvancedDisplay)
	bool bPrettyPrintCommentCacheJSON;

	/** Commments will detect and add nodes are underneath on creation */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bDetectNodesContainedForNewComments;

	/** Mouse input chord to resize a node */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	FInputChord ResizeChord;

	/** Input key to enable comment controls */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	FInputChord EnableCommentControlsKey;

	/** Collision method to use when resizing comment nodes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	ECommentCollisionMethod ResizeCollisionMethod;

	/** Collision method to use when releasing alt */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	ECommentCollisionMethod AltCollisionMethod;

	/** Snap to the grid when resizing the node */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bSnapToGridWhileResizing;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bIgnoreKnotNodes;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bIgnoreKnotNodesWhenPressingAlt;

	/** Don't add knot nodes to comment boxes */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bIgnoreKnotNodesWhenResizing;

	/** Don't snap to selected nodes on creation */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bIgnoreSelectedNodesOnCreation;

	/** Refresh the nodes inside the comment when you start moving the comment */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bRefreshContainingNodesOnMove;

	/** Disable the tooltip when hovering the titlebar */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bDisableTooltip;

	/** Highlight the contained node for a comment when you select it */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bHighlightContainingNodesOnSelection;

	/** Force the graph panel to use the 1:1 LOD for nodes (UE 5.0+) */
	UPROPERTY(EditAnywhere, config, Category = Misc)
	bool bUseMaxDetailNodes;

	/** Do not use ASC node for these graphs, turn on DebugClass_ASC and open graph to find graph class name */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay)
	TArray<FString> IgnoredGraphs;

	/** Override settings (resizing mode) for these graph types */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay, meta=(ForceInlineRow))
	TMap<FName, FASCGraphSettings> GraphSettingsOverride;

	/** Hide prompt for suggested settings with Blueprint Assist plugin */
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay)
	bool bSuppressSuggestedSettings;

	/** Hide prompt for suggested settings when source control is enabled*/
	UPROPERTY(EditAnywhere, config, Category = Misc, AdvancedDisplay)
	bool bSuppressSourceControlNotification;

	/** Size of the corner resizing anchors */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	float ResizeCornerAnchorSize;

	/** Padding to activate resizing on the side of a comment */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	float ResizeSidePadding;

	/** Hide the resize button */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHideResizeButton;

	/** Hide the header button */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHideHeaderButton;

	/** Hide the preset buttons */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHidePresets;

	/** Hide the randomize color button */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHideRandomizeButton;

	/** Hide controls at the bottom of the comment box */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHideCommentBoxControls;

	/** Hide the corner points (resize still enabled) */
	UPROPERTY(EditAnywhere, config, Category = Controls)
	bool bHideCornerPoints;

	/** Experimental fix for sort depth issue in UE5 (unable to move nested nodes until you compile the blueprint) */
	UPROPERTY(EditAnywhere, config, Category = Experimental)
	bool bEnableFixForSortDepthIssue;

	/** Print info about the graph when opening a graph */
	UPROPERTY(EditAnywhere, config, Category = Debug)
	bool bDebugGraph_ASC;

	UPROPERTY(EditAnywhere, config, Category = Debug)
	bool bDisablePackageCleanup;

	/** Use the default Unreal comment node */
	UPROPERTY(EditAnywhere, config, Category = Debug)
	bool bDisableASCGraphNode;

	static FORCEINLINE const UAutoSizeCommentsSettings& Get()
	{
		return *GetDefault<UAutoSizeCommentsSettings>();
	}

	static FORCEINLINE UAutoSizeCommentsSettings& GetMutable()
	{
		return *GetMutableDefault<UAutoSizeCommentsSettings>();
	}
};

class FASCSettingsDetails final : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

