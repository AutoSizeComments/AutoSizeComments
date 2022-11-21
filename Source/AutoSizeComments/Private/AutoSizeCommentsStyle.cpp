#include "AutoSizeCommentsStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#if ENGINE_MAJOR_VERSION >= 5
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
