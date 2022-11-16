// Copyright 2021 fpwong. All Rights Reserved.

#pragma once

#define ASC_UE_VERSION_OR_LATER(major, minor) (ENGINE_MAJOR_VERSION == major && ENGINE_MINOR_VERSION >= minor) || ENGINE_MAJOR_VERSION > major

#if ASC_UE_VERSION_OR_LATER(5, 0)
#define ASC_SUBOBJECT_EDITOR_TREE_NODE FSubobjectEditorTreeNode
#define ASC_SUBOBJECT_EDITOR SSubobjectEditor
#define ASC_GET_STYLE_SET_NAME FAppStyle::GetAppStyleSetName
#define ASC_STYLE_CLASS FAppStyle
#else
#define ASC_SUBOBJECT_EDITOR_TREE_NODE FSCSEditorTreeNode
#define ASC_SUBOBJECT_EDITOR SSCSEditor
#define ASC_GET_STYLE_SET_NAME FEditorStyle::GetStyleSetName
#define ASC_STYLE_CLASS FEditorStyle
#endif

#if ASC_UE_VERSION_OR_LATER(5, 1)
#define ASC_GET_FONT_STYLE FAppStyle::GetFontStyle
#else
#define ASC_GET_FONT_STYLE FEditorStyle::GetFontStyle
#endif