/////////////////////////////////////////////////////////////////////////////
// Name:        lottiedevicecontext.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "lottiedevicecontext.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cassert>

//----------------------------------------------------------------------------

#include "atts_shared.h"
#include "glyph.h"
#include "object.h"
#include "svgpathparser.h"
#include "vrv.h"

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// Local helpers
//----------------------------------------------------------------------------

namespace {

    LottieVec ToVec(int x, int y)
    {
        return LottieVec{ double(x), double(y) };
    }

    LottieVec ToVec(const Point &p)
    {
        return ToVec(p.x, p.y);
    }

    LottieVec Sub(const LottieVec &a, const LottieVec &b)
    {
        return LottieVec{ a.x - b.x, a.y - b.y };
    }

    LottieVec Scale(const LottieVec &v, double factor)
    {
        return LottieVec{ v.x * factor, v.y * factor };
    }

    // A subpath made of straight segments only (no curve tangents).
    LottieBezier MakeStraightBezier(const std::vector<LottieVec> &vertices, bool closed)
    {
        LottieBezier bezier;
        bezier.v = vertices;
        bezier.i.assign(vertices.size(), LottieVec());
        bezier.o.assign(vertices.size(), LottieVec());
        bezier.closed = closed;
        return bezier;
    }

    // The SVG output always has a visible outline (global CSS: "stroke:currentColor"), so every
    // shape in the Lottie IR carries an explicit stroke too. Width defaults to 1 (SVG default),
    // color/opacity stay COLOR_NONE / default so they are inherited from the enclosing group.
    void ApplyStrokeFromPen(LottieShape &shape, const Pen &pen)
    {
        shape.hasStroke = true;
        shape.strokeWidth = (pen.GetWidth() > 0) ? pen.GetWidth() : 1;
        shape.strokeColor = pen.HasColor() ? pen.GetColor() : COLOR_NONE;
        if (pen.HasOpacity()) {
            shape.strokeOpacity = pen.GetOpacity();
        }
        shape.lineCap = pen.GetLineCap();
        shape.lineJoin = pen.GetLineJoin();
    }

    // Only DrawLine, DrawPolyline and DrawPolygon reproduce dashing in SVG (DrawRoundedRectangle
    // and DrawEllipse never call AppendStrokeDashArray there either).
    void ApplyDashFromPen(LottieShape &shape, const Pen &pen)
    {
        if (pen.GetDashLength() > 0) {
            shape.dashLength = pen.GetDashLength();
            shape.gapLength = (pen.GetGapLength() > 0) ? pen.GetGapLength() : pen.GetDashLength();
        }
    }

    void ApplyFillFromBrush(LottieShape &shape, const Brush &brush)
    {
        shape.hasFill = true;
        shape.fillColor = brush.HasColor() ? brush.GetColor() : COLOR_NONE;
        if (brush.HasOpacity()) {
            shape.fillOpacity = brush.GetOpacity();
        }
    }

} // namespace

//----------------------------------------------------------------------------
// LottieDeviceContext
//----------------------------------------------------------------------------

LottieDeviceContext::LottieDeviceContext() : DeviceContext(LOTTIE_DEVICE_CONTEXT) {}

LottieDeviceContext::~LottieDeviceContext() {}

void LottieDeviceContext::SetBackground(int color, int style) {}

void LottieDeviceContext::SetBackgroundImage(void *image, double opacity) {}

void LottieDeviceContext::SetBackgroundMode(int mode) {}

void LottieDeviceContext::SetTextForeground(int color) {}

void LottieDeviceContext::SetTextBackground(int color) {}

void LottieDeviceContext::SetLogicalOrigin(int x, int y)
{
    m_originX = -x;
    m_originY = -y;
}

Point LottieDeviceContext::GetLogicalOrigin()
{
    return Point(m_originX, m_originY);
}

void LottieDeviceContext::DrawQuadBezierPath(Point bezier[3])
{
    assert(!m_penStack.empty());
    const Pen &currentPen = m_penStack.top();

    const LottieVec p0 = ToVec(bezier[0]);
    const LottieVec p1 = ToVec(bezier[1]);
    const LottieVec p2 = ToVec(bezier[2]);

    LottieBezier path = MakeStraightBezier({ p0, p2 }, false);
    // Degree-elevated to a cubic (C1 = P0 + 2/3(P1-P0), C2 = P2 + 2/3(P1-P2)); as a tangent
    // relative to its vertex, C1-P0 and C2-P2 reduce to 2/3 of P1-P0 and P1-P2.
    path.o[0] = Scale(Sub(p1, p0), 2.0 / 3.0);
    path.i[1] = Scale(Sub(p1, p2), 2.0 / 3.0);

    LottieShape shape;
    shape.paths.push_back(std::move(path));

    // fill="none" in SVG: no ApplyFillFromBrush call, hasFill stays false.
    ApplyStrokeFromPen(shape, currentPen);
    // linecap/linejoin are fixed to round in SVG, regardless of the pen.
    shape.lineCap = LINECAP_ROUND;
    shape.lineJoin = LINEJOIN_ROUND;
    ApplyDashFromPen(shape, currentPen);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawCubicBezierPath(Point bezier[4])
{
    assert(!m_penStack.empty());
    const Pen &currentPen = m_penStack.top();

    const LottieVec p0 = ToVec(bezier[0]);
    const LottieVec p1 = ToVec(bezier[1]);
    const LottieVec p2 = ToVec(bezier[2]);
    const LottieVec p3 = ToVec(bezier[3]);

    LottieBezier path = MakeStraightBezier({ p0, p3 }, false);
    path.o[0] = Sub(p1, p0);
    path.i[1] = Sub(p2, p3);

    LottieShape shape;
    shape.paths.push_back(std::move(path));

    // fill="none" in SVG: no ApplyFillFromBrush call, hasFill stays false.
    ApplyStrokeFromPen(shape, currentPen);
    shape.lineCap = LINECAP_ROUND;
    shape.lineJoin = LINEJOIN_ROUND;
    ApplyDashFromPen(shape, currentPen);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawCubicBezierPathFilled(Point bezier1[4], Point bezier2[4])
{
    assert(!m_penStack.empty());
    assert(!m_brushStack.empty());

    const Pen &currentPen = m_penStack.top();
    const Brush &currentBrush = m_brushStack.top();

    const LottieVec b1_0 = ToVec(bezier1[0]);
    const LottieVec b1_1 = ToVec(bezier1[1]);
    const LottieVec b1_2 = ToVec(bezier1[2]);
    const LottieVec b1_3 = ToVec(bezier1[3]);
    const LottieVec b2_0 = ToVec(bezier2[0]);
    const LottieVec b2_1 = ToVec(bezier2[1]);
    const LottieVec b2_2 = ToVec(bezier2[2]);

    // SVG: M b1[0] C b1[1] b1[2] b1[3] C b2[2] b2[1] b2[0] (no Z; the closing segment
    // b2[0]->b1[0] only exists in Lottie, where every subpath must state c=true explicitly).
    LottieBezier path = MakeStraightBezier({ b1_0, b1_3, b2_0 }, true);
    path.o[0] = Sub(b1_1, b1_0);
    path.i[1] = Sub(b1_2, b1_3);
    path.o[1] = Sub(b2_2, b1_3);
    path.i[2] = Sub(b2_1, b2_0);
    // i[0] and o[2] stay (0,0): the Lottie-only closing segment is straight.

    LottieShape shape;
    shape.paths.push_back(std::move(path));

    ApplyStrokeFromPen(shape, currentPen);
    shape.lineCap = LINECAP_ROUND;
    shape.lineJoin = LINEJOIN_ROUND;
    ApplyFillFromBrush(shape, currentBrush);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawBentParallelogramFilled(Point side[4], int height)
{
    assert(!m_penStack.empty());
    assert(!m_brushStack.empty());

    const Pen &currentPen = m_penStack.top();
    const Brush &currentBrush = m_brushStack.top();

    const LottieVec s0 = ToVec(side[0]);
    const LottieVec s1 = ToVec(side[1]);
    const LottieVec s2 = ToVec(side[2]);
    const LottieVec s3 = ToVec(side[3]);
    const LottieVec s0h{ s0.x, s0.y + height };
    const LottieVec s3h{ s3.x, s3.y + height };

    // SVG: M s0 C s1 s2 s3 L s3+h C (s2+h)(s1+h)(s0+h) Z.
    LottieBezier path = MakeStraightBezier({ s0, s3, s3h, s0h }, true);
    path.o[0] = Sub(s1, s0);
    path.i[1] = Sub(s2, s3);
    // o[1]/i[2] stay (0,0): s3 -> s3+h is the straight "L" segment.
    path.o[2] = Sub(s2, s3);
    path.i[3] = Sub(s1, s0);
    // o[3]/i[0] stay (0,0): the closing "Z" segment (s0+h -> s0) is straight.

    LottieShape shape;
    shape.paths.push_back(std::move(path));

    ApplyStrokeFromPen(shape, currentPen);
    shape.lineCap = LINECAP_ROUND;
    shape.lineJoin = LINEJOIN_ROUND;
    ApplyFillFromBrush(shape, currentBrush);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawCircle(int x, int y, int radius)
{
    this->DrawEllipse(x - radius, y - radius, 2 * radius, 2 * radius);
}

void LottieDeviceContext::DrawEllipse(int x, int y, int width, int height)
{
    assert(!m_penStack.empty());
    assert(!m_brushStack.empty());

    const Pen &currentPen = m_penStack.top();
    const Brush &currentBrush = m_brushStack.top();

    // Integer division on purpose: it reproduces the same half-pixel truncation as
    // SvgDeviceContext::DrawEllipse, which also declares rw/rh as int.
    const int rw = width / 2;
    const int rh = height / 2;

    LottieShape shape;
    shape.kind = LottieShapeKind::Ellipse;
    shape.center = ToVec(x + rw, y + rh);
    shape.size = ToVec(2 * rw, 2 * rh);

    ApplyStrokeFromPen(shape, currentPen);
    ApplyFillFromBrush(shape, currentBrush);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawEllipticArc(int x, int y, int width, int height, double start, double end) {}

void LottieDeviceContext::DrawLine(int x1, int y1, int x2, int y2)
{
    assert(!m_penStack.empty());
    const Pen &currentPen = m_penStack.top();

    LottieShape shape;
    shape.paths.push_back(MakeStraightBezier({ ToVec(x1, y1), ToVec(x2, y2) }, false));

    ApplyStrokeFromPen(shape, currentPen);
    ApplyDashFromPen(shape, currentPen);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawPolyline(int n, Point points[], bool close)
{
    assert(!m_penStack.empty());
    const Pen &currentPen = m_penStack.top();

    std::vector<LottieVec> vertices;
    vertices.reserve(n);
    for (int i = 0; i < n; ++i) {
        vertices.push_back(ToVec(points[i]));
    }

    LottieShape shape;
    // No fill: mirrors SvgDeviceContext::DrawPolyline, which never sets a fill color and only
    // forces fill="none" explicitly when n > 2 (for n <= 2 the enclosed area is zero anyway).
    shape.paths.push_back(MakeStraightBezier(vertices, close));

    ApplyStrokeFromPen(shape, currentPen);
    ApplyDashFromPen(shape, currentPen);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawPolygon(int n, Point points[])
{
    assert(!m_penStack.empty());
    assert(!m_brushStack.empty());

    const Pen &currentPen = m_penStack.top();
    const Brush &currentBrush = m_brushStack.top();

    std::vector<LottieVec> vertices;
    vertices.reserve(n);
    for (int i = 0; i < n; ++i) {
        vertices.push_back(ToVec(points[i]));
    }

    LottieShape shape;
    shape.paths.push_back(MakeStraightBezier(vertices, true));

    ApplyStrokeFromPen(shape, currentPen);
    ApplyDashFromPen(shape, currentPen);
    ApplyFillFromBrush(shape, currentBrush);

    this->AddShape(std::move(shape));
}

void LottieDeviceContext::DrawRectangle(int x, int y, int width, int height)
{
    this->DrawRoundedRectangle(x, y, width, height, 0);
}

void LottieDeviceContext::DrawRotatedText(const std::string &text, int x, int y, double angle) {}

void LottieDeviceContext::DrawRoundedRectangle(int x, int y, int width, int height, int radius)
{
    assert(!m_penStack.empty());
    assert(!m_brushStack.empty());

    const Pen &currentPen = m_penStack.top();
    const Brush &currentBrush = m_brushStack.top();

    // Negative heights or widths are not allowed in SVG; normalize the same way
    // SvgDeviceContext::DrawRoundedRectangle does.
    if (height < 0) {
        height = -height;
        y -= height;
    }
    if (width < 0) {
        width = -width;
        x -= width;
    }

    LottieShape shape;
    shape.kind = LottieShapeKind::Rect;
    // Exact (non-truncated) center: unlike DrawEllipse, the SVG <rect> keeps x/y/width/height
    // untouched, so converting to Lottie's center+size form must not introduce new rounding.
    shape.center = LottieVec{ x + width / 2.0, y + height / 2.0 };
    shape.size = ToVec(width, height);
    shape.radius = radius;

    ApplyStrokeFromPen(shape, currentPen);
    ApplyFillFromBrush(shape, currentBrush);

    this->AddShape(std::move(shape));
}

LottieShape LottieDeviceContext::MakeGlyphShape(const Glyph *glyph, const FontInfo *font, int x, int y)
{
    assert(glyph);
    assert(font);

    std::map<const Glyph *, std::vector<LottieBezier>>::iterator cacheIt = m_glyphCache.find(glyph);
    if (cacheIt == m_glyphCache.end()) {
        std::vector<LottieBezier> parsedPaths;
        ParseGlyphXml(glyph->GetXML(), parsedPaths);
        cacheIt = m_glyphCache.emplace(glyph, std::move(parsedPaths)).first;
    }

    double scaleX = (double)font->GetPointSize() / glyph->GetUnitsPerEm() * DEFINITION_FACTOR;
    double scaleY = scaleX;
    if (font->GetWidthToHeightRatio() != 1.0f) scaleX *= font->GetWidthToHeightRatio();

    LottieShape shape;
    shape.paths.reserve(cacheIt->second.size());
    for (const LottieBezier &glyphPath : cacheIt->second) {
        LottieBezier path;
        path.closed = glyphPath.closed;
        const std::size_t count = glyphPath.v.size();
        path.v.reserve(count);
        path.i.reserve(count);
        path.o.reserve(count);
        for (std::size_t idx = 0; idx < count; ++idx) {
            path.v.push_back(LottieVec{ x + scaleX * glyphPath.v[idx].x, y + scaleY * glyphPath.v[idx].y });
            path.i.push_back(LottieVec{ scaleX * glyphPath.i[idx].x, scaleY * glyphPath.i[idx].y });
            path.o.push_back(LottieVec{ scaleX * glyphPath.o[idx].x, scaleY * glyphPath.o[idx].y });
        }
        shape.paths.push_back(std::move(path));
    }

    // Fill and stroke both inherited (CSS "path {stroke:currentColor}" plus the ancestor
    // group's "fill" attribute, same as every other glyph-less shape's COLOR_NONE).
    // Stroke width mirrors the SVG default (1, in glyph units) scaled like the geometry.
    shape.hasFill = true;
    shape.hasStroke = true;
    shape.strokeWidth = scaleY;

    return shape;
}

int LottieDeviceContext::GetGlyphAdvance(const Glyph *glyph, const FontInfo *font)
{
    assert(glyph);
    assert(font);

    // Exact same integer arithmetic as SvgDeviceContext::DrawMusicText, to keep advance
    // widths pixel-identical between the two outputs.
    if (glyph->GetHorizAdvX() > 0) {
        return glyph->GetHorizAdvX() * font->GetPointSize() / glyph->GetUnitsPerEm();
    }
    int gx, gy, w, h;
    glyph->GetBoundingBox(gx, gy, w, h);
    return w * font->GetPointSize() / glyph->GetUnitsPerEm();
}

void LottieDeviceContext::DrawText(
    const std::string &text, const std::u32string &wtext, int x, int y, int width, int height)
{
    assert(!m_fontStack.empty());
    FontInfo *font = m_fontStack.top();

    // Mirrors SvgDeviceContext::DrawText L1163-1166: an explicit x/y without width/height moves
    // the pen without starting a new anchored chunk (e.g. DrawLyricString positioning a
    // syllable). The width/height-only case (invisible sylTextRect) is not visual and is
    // skipped entirely, without moving the pen, exactly like the SVG in that branch.
    const bool hasPosition = (x != 0) && (y != 0) && (x != VRV_UNSET) && (y != VRV_UNSET);
    const bool hasSize = (width != 0) && (height != 0) && (width != VRV_UNSET) && (height != VRV_UNSET);
    if (hasPosition && !hasSize) {
        m_textPenX = x;
        m_textPenY = y;
    }

    std::u32string chars = wtext;
    if (chars.empty() && !text.empty()) {
        chars.assign(text.begin(), text.end());
    }
    if (chars.empty()) return;

    const int letterSpacing = font->GetLetterSpacing();

    if (font->GetSmuflFont() != SMUFL_NONE) {
        const Resources *resources = this->GetResources();
        assert(resources);

        bool first = true;
        for (char32_t c : chars) {
            // Letter-spacing is a per-run CSS property in the SVG (set on the <tspan>), so it
            // is applied between characters of this call only, not carried over from a
            // previous DrawText call in the same chunk.
            if (!first && letterSpacing != 0) {
                m_textPenX += letterSpacing;
                m_textChunkWidth += letterSpacing;
            }
            first = false;

            const Glyph *glyph = resources->GetGlyph(c);
            if (!glyph) continue;

            m_textChunkShapes.push_back(this->MakeGlyphShape(glyph, font, m_textPenX, m_textPenY));

            const int advance = this->GetGlyphAdvance(glyph, font);
            m_textPenX += advance;
            m_textChunkWidth += advance;
        }
    }
    else {
        // Common (non-SMuFL) text is not rendered until D-TEXTO; only advance the pen so
        // subsequent SMuFL runs in the same chunk (e.g. a dynamic mixing letters and glyphs)
        // stay correctly positioned, and count the skipped run for the EndPage warning.
        TextExtend extend;
        this->GetTextExtent(chars, &extend, true);
        m_textPenX += extend.m_width;
        m_textChunkWidth += extend.m_width;
        ++m_skippedTextRuns;
    }
}

void LottieDeviceContext::DrawMusicText(const std::u32string &text, int x, int y, bool setSmuflGlyph)
{
    assert(!m_fontStack.empty());
    FontInfo *font = m_fontStack.top();

    const Resources *resources = this->GetResources();
    assert(resources);

    for (char32_t c : text) {
        const Glyph *glyph = resources->GetGlyph(c);
        if (!glyph) {
            continue;
        }

        this->AddShape(this->MakeGlyphShape(glyph, font, x, y));
        x += this->GetGlyphAdvance(glyph, font);
    }
}

void LottieDeviceContext::DrawSpline(int n, Point points[]) {}

void LottieDeviceContext::DrawGraphicUri(int x, int y, int width, int height, const std::string &uri) {}

void LottieDeviceContext::DrawSvgShape(int x, int y, int width, int height, double scale, pugi::xml_node svg) {}

void LottieDeviceContext::DrawBackgroundImage(int x, int y) {}

void LottieDeviceContext::StartText(int x, int y, data_HORIZONTALALIGNMENT alignment)
{
    m_textPenX = x;
    m_textPenY = y;
    m_textAlignment = alignment;
    m_textChunkShapes.clear();
    m_textChunkWidth = 0.0;
}

void LottieDeviceContext::EndText()
{
    this->FinalizeTextChunk();
}

void LottieDeviceContext::MoveTextTo(int x, int y, data_HORIZONTALALIGNMENT alignment)
{
    // In the SVG, an absolute x/y starts a new anchored text chunk (finalize the pending one
    // before moving the pen). An HORIZONTALALIGNMENT_NONE here (e.g. explicit repositioning
    // after an <lb/>) means the SVG only sets x/y and keeps the current text-anchor, so keep
    // the current alignment rather than resetting it.
    this->FinalizeTextChunk();
    m_textPenX = x;
    m_textPenY = y;
    if (alignment != HORIZONTALALIGNMENT_NONE) {
        m_textAlignment = alignment;
    }
}

void LottieDeviceContext::MoveTextVerticallyTo(int y)
{
    m_textPenY = y;
}

void LottieDeviceContext::FinalizeTextChunk()
{
    if (m_textChunkShapes.empty()) {
        m_textChunkWidth = 0.0;
        return;
    }

    double offset = 0.0;
    if (m_textAlignment == HORIZONTALALIGNMENT_center) {
        offset = -m_textChunkWidth / 2.0;
    }
    else if (m_textAlignment == HORIZONTALALIGNMENT_right) {
        offset = -m_textChunkWidth;
    }

    for (LottieShape &shape : m_textChunkShapes) {
        if (offset != 0.0) {
            for (LottieBezier &path : shape.paths) {
                for (LottieVec &vertex : path.v) {
                    vertex.x += offset;
                }
            }
        }
        this->AddShape(std::move(shape));
    }

    m_textChunkShapes.clear();
    m_textChunkWidth = 0.0;
}

void LottieDeviceContext::StartGraphic(
    Object *object, const std::string &gClass, const std::string &gId, GraphicID graphicID, bool prepend)
{
    assert(!m_nodeStack.empty());

    std::unique_ptr<LottieNode> node = std::make_unique<LottieNode>();
    node->id = (graphicID == PRIMARY) ? gId : "";
    node->className = object->GetClassName();
    if (!gClass.empty()) {
        node->className.append(" " + gClass);
    }

    if (object->HasAttClass(ATT_COLOR)) {
        AttColor *att = dynamic_cast<AttColor *>(object);
        assert(att);
        if (att->HasColor()) {
            node->colorCss = att->GetColor();
        }
    }

    if (object->HasAttClass(ATT_VISIBILITY)) {
        AttVisibility *att = dynamic_cast<AttVisibility *>(object);
        assert(att);
        if (att->HasVisible() && (att->GetVisible() == BOOLEAN_false)) {
            node->hidden = true;
        }
    }

    LottieNode *current = m_nodeStack.back();
    LottieNode *added = node.get();

    LottieChild child;
    child.group = std::move(node);

    if (prepend) {
        current->children.insert(current->children.begin(), std::move(child));
    }
    else {
        current->children.push_back(std::move(child));
    }

    m_nodeStack.push_back(added);

    if (!added->id.empty()) {
        m_idMap[added->id] = added;
    }
}

void LottieDeviceContext::EndGraphic(Object *object, View *view)
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::StartCustomGraphic(const std::string &name, std::string gClass, std::string gId)
{
    assert(!m_nodeStack.empty());

    std::unique_ptr<LottieNode> node = std::make_unique<LottieNode>();
    node->id = gId;
    node->className = name;
    if (!gClass.empty()) {
        node->className.append(" " + gClass);
    }

    LottieNode *current = m_nodeStack.back();
    LottieNode *added = node.get();

    LottieChild child;
    child.group = std::move(node);
    current->children.push_back(std::move(child));

    m_nodeStack.push_back(added);

    if (!added->id.empty()) {
        m_idMap[added->id] = added;
    }
}

void LottieDeviceContext::EndCustomGraphic()
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::SetCustomGraphicColor(const std::string &color)
{
    assert(!m_nodeStack.empty());
    m_nodeStack.back()->colorCss = color;
}

void LottieDeviceContext::ResumeGraphic(Object *object, std::string gId)
{
    assert(!m_nodeStack.empty());

    std::map<std::string, LottieNode *>::iterator it = m_idMap.find(gId);
    if (it != m_idMap.end()) {
        m_nodeStack.push_back(it->second);
    }
    else {
        m_nodeStack.push_back(m_nodeStack.back());
    }
}

void LottieDeviceContext::EndResumedGraphic(Object *object, View *view)
{
    assert(m_nodeStack.size() > 1);
    m_nodeStack.pop_back();
}

void LottieDeviceContext::RotateGraphic(Point const &orig, double angle)
{
    assert(!m_nodeStack.empty());

    LottieNode *node = m_nodeStack.back();
    if (node->hasRotation) {
        return;
    }
    node->hasRotation = true;
    node->rotation = angle;
    node->rotationOrigin = orig;
}

void LottieDeviceContext::StartPage()
{
    LottiePage page;
    page.root = std::make_unique<LottieNode>();
    page.width = this->GetWidth();
    page.height = this->GetHeight();
    page.contentHeight = this->GetContentHeight();
    std::pair<int, int> baseSize = this->GetBaseSize();
    page.baseWidth = baseSize.first;
    page.baseHeight = baseSize.second;
    page.userScaleX = this->GetUserScaleX();
    page.userScaleY = this->GetUserScaleY();
    page.viewBoxFactor = this->GetViewBoxFactor();
    page.originX = m_originX;
    page.originY = m_originY;

    m_pages.push_back(std::move(page));
    m_idMap.clear();
    m_nodeStack.clear();
    m_nodeStack.push_back(m_pages.back().root.get());
}

void LottieDeviceContext::EndPage()
{
    assert(m_nodeStack.size() == 1);
    m_nodeStack.clear();

    if (m_skippedTextRuns > 0) {
        LogWarning(
            "LottieDeviceContext: %u common text run(s) not rendered (pending D-TEXTO)", m_skippedTextRuns);
        m_skippedTextRuns = 0;
    }
}

void LottieDeviceContext::AddShape(LottieShape &&shape)
{
    assert(!m_nodeStack.empty());

    LottieNode *node = m_nodeStack.back();

    std::vector<LottieChild>::iterator firstGroup = std::find_if(node->children.begin(), node->children.end(),
        [](const LottieChild &child) { return (child.group != NULL); });

    LottieChild child;
    child.shape = std::move(shape);

    if (firstGroup != node->children.end()) {
        node->children.insert(firstGroup, std::move(child));
    }
    else if (m_pushBack) {
        node->children.insert(node->children.begin(), std::move(child));
    }
    else {
        node->children.push_back(std::move(child));
    }
}

} // namespace vrv
