#include "AutoSizeCommentsStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#if ASC_UE_VERSION_OR_LATER(5, 0)
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"
#endif

TSharedPtr<FSlateStyleSet> FASCStyle::StyleSet = nullptr;

#define ASC_IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define ASC_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define ASC_BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define ASC_BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( StyleSet->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define ASC_DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define ASC_ICON_FONT(...) FSlateFontInfo(StyleSet->RootToContentDir("Fonts/FontAwesome", TEXT(".ttf")), __VA_ARGS__)

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

void FASCStyle::Initialize()
{
	// Only register once
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShareable(new FSlateStyleSet("AutoSizeCommentsStyle"));

	StyleSet->SetContentRoot(IPluginManager::Get().FindPlugin("AutoSizeComments")->GetBaseDir() / TEXT("Resources"));

	StyleSet->Set("ASC.AnchorBox", new ASC_BOX_BRUSH("AnchorBox", FMargin(18.0f/64.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

#if ASC_UE_VERSION_OR_LATER(5, 0)
	{
		const FTextBlockStyle GraphCommentBlockTitle = FTextBlockStyle()
			.SetFont(DEFAULT_FONT("Bold", 18))
			.SetColorAndOpacity(FLinearColor(218.0f / 255.0f, 218.0f / 255.0f, 218.0f / 255.0f))
			.SetShadowOffset( FVector2D(1.5f, 1.5f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f));

		FSlateBrush EmptyBrush;
		EmptyBrush.Margin = FMargin(0);
		EmptyBrush.DrawAs = ESlateBrushDrawType::Type::NoDrawType;

		FScrollBarStyle ScrollBarStyle;
		ScrollBarStyle.Thickness = 0;
		ScrollBarStyle.SetDraggedThumbImage(EmptyBrush);
		ScrollBarStyle.SetHoveredThumbImage(EmptyBrush);
		ScrollBarStyle.SetNormalThumbImage(EmptyBrush);

		const FEditableTextBoxStyle GraphCommentBlockTitleEditableText = FEditableTextBoxStyle()
			.SetPadding(FMargin(0))
			.SetHScrollBarPadding(FMargin(0))
			.SetVScrollBarPadding(FMargin(0))
			.SetBackgroundImageFocused(EmptyBrush)
			.SetBackgroundImageHovered(EmptyBrush)
			.SetBackgroundImageNormal(EmptyBrush)
			.SetBackgroundImageReadOnly(EmptyBrush)
			.SetScrollBarStyle(ScrollBarStyle)
			.SetFont(GraphCommentBlockTitle.Font)
			.SetFocusedForegroundColor(FColor(200, 200, 200, 255));

		const FInlineEditableTextBlockStyle InlineEditableTextBoxStyle = FInlineEditableTextBlockStyle()
			.SetTextStyle(GraphCommentBlockTitle)
			.SetEditableTextBoxStyle(GraphCommentBlockTitleEditableText);

		StyleSet->Set("ASC.CommentTitleTextBoxStyle", InlineEditableTextBoxStyle);
	}
#endif

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

#undef ASC_IMAGE_BRUSH
#undef ASC_BOX_BRUSH
#undef ASC_BORDER_BRUSH
#undef ASC_DEFAULT_FONT
#undef ASC_ICON_FONT

void FASCStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}

const ISlateStyle& FASCStyle::Get()
{
	return *(StyleSet.Get());
}

const FName& FASCStyle::GetStyleSetName()
{
	return StyleSet->GetStyleSetName();
}
