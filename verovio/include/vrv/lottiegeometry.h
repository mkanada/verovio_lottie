/////////////////////////////////////////////////////////////////////////////
// Name:        lottiegeometry.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_GEOMETRY_H__
#define __VRV_LOTTIE_GEOMETRY_H__

#include <memory>
#include <optional>
#include <string>
#include <vector>

//----------------------------------------------------------------------------

#include "devicecontextbase.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottieVec, LottieBezier, LottieShape
//----------------------------------------------------------------------------

struct LottieVec {
    double x = 0.0;
    double y = 0.0;
};

/**
 * A subpath in Lottie format: tangents are relative to their vertex.
 */
struct LottieBezier {
    std::vector<LottieVec> v; // vertices (page coordinates)
    std::vector<LottieVec> i; // in-tangent of each vertex
    std::vector<LottieVec> o; // out-tangent of each vertex
    bool closed = false;
};

enum class LottieShapeKind { Path, Rect, Ellipse };

struct LottieShape {
    LottieShapeKind kind = LottieShapeKind::Path;
    std::vector<LottieBezier> paths; // Path: subpaths share the same fill
    LottieVec center, size; // Rect / Ellipse
    double radius = 0.0; // rounded Rect
    bool hasFill = false;
    int fillColor = COLOR_NONE; // COLOR_NONE = inherit the group color
    double fillOpacity = 1.0;
    bool hasStroke = false;
    double strokeWidth = 1.0;
    int strokeColor = COLOR_NONE;
    double strokeOpacity = 1.0;
    LineCapStyle lineCap = LINECAP_DEFAULT;
    LineJoinStyle lineJoin = LINEJOIN_DEFAULT;
    double dashLength = 0.0;
    double gapLength = 0.0;
};

/**
 * A run of non-SMuFL ("common") text, e.g. a title, tempo mark or fingering, built by
 * LottieDeviceContext::DrawText (D01, docs/plano/D01-texto-comum.md) and serialized by
 * LottieWriter as a native Lottie text layer ("ty":5) instead of glyph shapes.
 */
struct LottieTextRun {
    std::u32string text;
    Point origin; // anchor (page px), before any alignment offset
    data_HORIZONTALALIGNMENT alignment = HORIZONTALALIGNMENT_left;
    double pointSize = 0.0; // same unit space as MakeGlyphShape (already page px)
    double letterSpacing = 0.0;
    data_FONTSTYLE style = FONTSTYLE_NONE;
    data_FONTWEIGHT weight = FONTWEIGHT_NONE;
    int color = COLOR_NONE; // COLOR_NONE = inherit, same convention as LottieShape::fillColor
};

//----------------------------------------------------------------------------
// LottieNode, LottieChild, LottiePage
//----------------------------------------------------------------------------

struct LottieNode;

struct LottieChild {
    std::unique_ptr<LottieNode> group; // non-null = subgroup
    std::optional<LottieTextRun> text; // set = common text run; otherwise a shape
    LottieShape shape; // used when group == nullptr and text == nullopt
};

struct LottieNode {
    std::string id; // xml:id (empty if not PRIMARY)
    std::string className; // Object::GetClassName() (+ extra classes) or the custom graphic name
    std::string colorCss; // @color or SetCustomGraphicColor; empty = inherit
    bool hidden = false;
    bool hasRotation = false;
    double rotation = 0.0;
    Point rotationOrigin;
    std::vector<LottieChild> children; // document order: later entries paint on top
};

struct LottiePage {
    std::unique_ptr<LottieNode> root;
    int width = 0, height = 0, contentHeight = 0;
    int baseWidth = 0, baseHeight = 0;
    double userScaleX = 1.0, userScaleY = 1.0;
    double viewBoxFactor = 10.0;
    int originX = 0, originY = 0;
};

} // namespace vrv

#endif // __VRV_LOTTIE_GEOMETRY_H__
