/////////////////////////////////////////////////////////////////////////////
// Name:        lottiewriter.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "lottiewriter.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// Local helpers
//----------------------------------------------------------------------------

// Fixed to 3 decimals, no trailing zeros, classic locale (dot decimal separator).
static std::string FormatNumber(double value)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(3) << value;
    std::string s = oss.str();

    std::size_t dot = s.find('.');
    if (dot != std::string::npos) {
        std::size_t last = s.find_last_not_of('0');
        if (last == dot) {
            --last;
        }
        s.erase(last + 1);
    }
    if (s == "-0") {
        s = "0";
    }
    return s;
}

static std::string EscapeJsonString(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if ((c == '"') || (c == '\\')) {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

// Only "#RRGGBB" and "#RGB" are supported at this stage (a full CSS color parser is A06).
static bool ParseHexColor(const std::string &css, int &outColor)
{
    auto isHex = [](char c) { return static_cast<bool>(std::isxdigit(static_cast<unsigned char>(c))); };

    if (css.empty() || (css[0] != '#')) {
        return false;
    }

    if (css.size() == 7) {
        if (!std::all_of(css.begin() + 1, css.end(), isHex)) {
            return false;
        }
        outColor = std::stoi(css.substr(1), nullptr, 16);
        return true;
    }

    if (css.size() == 4) {
        if (!std::all_of(css.begin() + 1, css.end(), isHex)) {
            return false;
        }
        const int r = std::stoi(std::string(2, css[1]), nullptr, 16);
        const int g = std::stoi(std::string(2, css[2]), nullptr, 16);
        const int b = std::stoi(std::string(2, css[3]), nullptr, 16);
        outColor = (r << 16) | (g << 8) | b;
        return true;
    }

    return false;
}

// Colors in the IR are packed 24-bit integers (COLOR_BLACK, COLOR_WHITE, parsed #RRGGBB, etc.).
static void ColorIntToRgb01(int color, double &r, double &g, double &b)
{
    r = ((color >> 16) & 0xFF) / 255.0;
    g = ((color >> 8) & 0xFF) / 255.0;
    b = (color & 0xFF) / 255.0;
}

static int ResolveColor(const std::string &colorCss, int inheritedColor)
{
    if (colorCss.empty()) {
        return inheritedColor;
    }
    int parsed = COLOR_NONE;
    if (ParseHexColor(colorCss, parsed)) {
        return parsed;
    }
    return inheritedColor;
}

static int MapLineCap(LineCapStyle cap)
{
    switch (cap) {
        case LINECAP_ROUND: return 2;
        case LINECAP_SQUARE: return 3;
        case LINECAP_DEFAULT:
        case LINECAP_BUTT:
        default: return 1;
    }
}

static int MapLineJoin(LineJoinStyle join)
{
    switch (join) {
        case LINEJOIN_ROUND: return 2;
        case LINEJOIN_BEVEL: return 3;
        case LINEJOIN_DEFAULT:
        case LINEJOIN_MITER:
        case LINEJOIN_MITER_CLIP:
        case LINEJOIN_ARCS:
        default: return 1;
    }
}

static std::string JoinItems(const std::vector<std::string> &items)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out << ",";
        out << items[i];
    }
    return out.str();
}

static std::string WriteTransformDefault()
{
    return "{\"ty\":\"tr\",\"p\":{\"a\":0,\"k\":[0,0]},\"a\":{\"a\":0,\"k\":[0,0]},\"s\":{\"a\":0,\"k\":[100,100]},"
           "\"r\":{\"a\":0,\"k\":0},\"o\":{\"a\":0,\"k\":100},\"sk\":{\"a\":0,\"k\":0},\"sa\":{\"a\":0,\"k\":0}}";
}

static std::string WriteTransformWithRotation(const Point &origin, double rotation)
{
    std::ostringstream out;
    out << "{\"ty\":\"tr\",\"p\":{\"a\":0,\"k\":[" << FormatNumber(origin.x) << "," << FormatNumber(origin.y)
        << "]},\"a\":{\"a\":0,\"k\":[" << FormatNumber(origin.x) << "," << FormatNumber(origin.y)
        << "]},\"s\":{\"a\":0,\"k\":[100,100]},\"r\":{\"a\":0,\"k\":" << FormatNumber(rotation)
        << "},\"o\":{\"a\":0,\"k\":100},\"sk\":{\"a\":0,\"k\":0},\"sa\":{\"a\":0,\"k\":0}}";
    return out.str();
}

static std::string WriteFill(const LottieShape &shape, int inheritedColor)
{
    const int color = (shape.fillColor == COLOR_NONE) ? inheritedColor : shape.fillColor;
    double r, g, b;
    ColorIntToRgb01(color, r, g, b);

    std::ostringstream out;
    out << "{\"ty\":\"fl\",\"c\":{\"a\":0,\"k\":[" << FormatNumber(r) << "," << FormatNumber(g) << ","
        << FormatNumber(b) << ",1]},\"o\":{\"a\":0,\"k\":" << FormatNumber(shape.fillOpacity * 100) << "},\"r\":1}";
    return out.str();
}

static std::string WriteStroke(const LottieShape &shape, int inheritedColor)
{
    const int color = (shape.strokeColor == COLOR_NONE) ? inheritedColor : shape.strokeColor;
    double r, g, b;
    ColorIntToRgb01(color, r, g, b);

    std::ostringstream out;
    out << "{\"ty\":\"st\",\"c\":{\"a\":0,\"k\":[" << FormatNumber(r) << "," << FormatNumber(g) << ","
        << FormatNumber(b) << ",1]},\"o\":{\"a\":0,\"k\":" << FormatNumber(shape.strokeOpacity * 100)
        << "},\"w\":{\"a\":0,\"k\":" << FormatNumber(shape.strokeWidth) << "},\"lc\":" << MapLineCap(shape.lineCap)
        << ",\"lj\":" << MapLineJoin(shape.lineJoin) << ",\"ml\":4";
    if (shape.dashLength > 0) {
        out << ",\"d\":[{\"n\":\"d\",\"nm\":\"dash\",\"v\":{\"a\":0,\"k\":" << FormatNumber(shape.dashLength)
            << "}},{\"n\":\"g\",\"nm\":\"gap\",\"v\":{\"a\":0,\"k\":" << FormatNumber(shape.gapLength) << "}}]";
    }
    out << "}";
    return out.str();
}

static std::string WriteBezier(const LottieBezier &bezier)
{
    std::ostringstream out;
    out << "{\"ty\":\"sh\",\"ks\":{\"a\":0,\"k\":{\"i\":[";
    for (std::size_t i = 0; i < bezier.i.size(); ++i) {
        if (i) out << ",";
        out << "[" << FormatNumber(bezier.i[i].x) << "," << FormatNumber(bezier.i[i].y) << "]";
    }
    out << "],\"o\":[";
    for (std::size_t i = 0; i < bezier.o.size(); ++i) {
        if (i) out << ",";
        out << "[" << FormatNumber(bezier.o[i].x) << "," << FormatNumber(bezier.o[i].y) << "]";
    }
    out << "],\"v\":[";
    for (std::size_t i = 0; i < bezier.v.size(); ++i) {
        if (i) out << ",";
        out << "[" << FormatNumber(bezier.v[i].x) << "," << FormatNumber(bezier.v[i].y) << "]";
    }
    out << "],\"c\":" << (bezier.closed ? "true" : "false") << "}}}";
    return out.str();
}

static std::string WriteRect(const LottieShape &shape)
{
    std::ostringstream out;
    out << "{\"ty\":\"rc\",\"p\":{\"a\":0,\"k\":[" << FormatNumber(shape.center.x) << ","
        << FormatNumber(shape.center.y) << "]},\"s\":{\"a\":0,\"k\":[" << FormatNumber(shape.size.x) << ","
        << FormatNumber(shape.size.y) << "]},\"r\":{\"a\":0,\"k\":" << FormatNumber(shape.radius) << "}}";
    return out.str();
}

static std::string WriteEllipse(const LottieShape &shape)
{
    std::ostringstream out;
    out << "{\"ty\":\"el\",\"p\":{\"a\":0,\"k\":[" << FormatNumber(shape.center.x) << ","
        << FormatNumber(shape.center.y) << "]},\"s\":{\"a\":0,\"k\":[" << FormatNumber(shape.size.x) << ","
        << FormatNumber(shape.size.y) << "]}}";
    return out.str();
}

static std::string WriteShapeGroup(const LottieShape &shape, int inheritedColor)
{
    std::vector<std::string> items;

    switch (shape.kind) {
        case LottieShapeKind::Path:
            for (const LottieBezier &bezier : shape.paths) {
                items.push_back(WriteBezier(bezier));
            }
            break;
        case LottieShapeKind::Rect: items.push_back(WriteRect(shape)); break;
        case LottieShapeKind::Ellipse: items.push_back(WriteEllipse(shape)); break;
    }

    // Stroke before fill so that it paints on top, as in SVG.
    if (shape.hasStroke) {
        items.push_back(WriteStroke(shape, inheritedColor));
    }
    if (shape.hasFill) {
        items.push_back(WriteFill(shape, inheritedColor));
    }
    items.push_back(WriteTransformDefault());

    return "{\"ty\":\"gr\",\"it\":[" + JoinItems(items) + "]}";
}

static std::string WriteNodeGroup(const LottieNode &node, int inheritedColor);

// Children are written from last to first: in the SVG/IR document order the later sibling
// paints on top, while in Lottie the first item of "it" paints on top.
static void AppendChildrenReversed(
    const std::vector<LottieChild> &children, int inheritedColor, std::vector<std::string> &items)
{
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (it->group) {
            if (it->group->hidden) {
                continue;
            }
            items.push_back(WriteNodeGroup(*it->group, inheritedColor));
        }
        else {
            items.push_back(WriteShapeGroup(it->shape, inheritedColor));
        }
    }
}

static std::string WriteNodeGroup(const LottieNode &node, int inheritedColor)
{
    const int nodeColor = ResolveColor(node.colorCss, inheritedColor);

    std::vector<std::string> items;
    AppendChildrenReversed(node.children, nodeColor, items);
    items.push_back(node.hasRotation ? WriteTransformWithRotation(node.rotationOrigin, node.rotation)
                                      : WriteTransformDefault());

    const std::string &nm = !node.id.empty() ? node.id : node.className;

    return "{\"ty\":\"gr\",\"nm\":\"" + EscapeJsonString(nm) + "\",\"mn\":\"" + EscapeJsonString(node.className)
        + "\",\"it\":[" + JoinItems(items) + "]}";
}

static std::string WriteLayerShapes(const LottiePage &page)
{
    std::vector<std::string> items;
    const int rootColor = ResolveColor(page.root->colorCss, COLOR_BLACK);
    AppendChildrenReversed(page.root->children, rootColor, items);
    return JoinItems(items);
}

//----------------------------------------------------------------------------
// PageMetrics
//----------------------------------------------------------------------------

// Reproduces SvgDeviceContext::Commit (non-mm branch) and StartPage (internal viewBox and
// preserveAspectRatio="xMidYMid meet" scaling of the inner <svg>).
namespace {
    struct PageMetrics {
        int wpx = 0;
        int hpx = 0;
        double scale = 1.0;
        double tx = 0.0;
        double ty = 0.0;
    };
} // namespace

static PageMetrics ComputePageMetrics(const LottiePage &page)
{
    PageMetrics metrics;

    if (page.baseWidth && page.baseHeight) {
        metrics.wpx = page.baseWidth;
        metrics.hpx = page.baseHeight;
    }
    else {
        metrics.wpx = static_cast<int>(std::ceil(page.width * page.userScaleX));
        metrics.hpx = static_cast<int>(std::ceil(page.height * page.userScaleY));
    }

    const int vw = static_cast<int>(page.width * page.viewBoxFactor);
    const int vh = static_cast<int>(page.contentHeight * page.viewBoxFactor);

    const double scaleX = (vw != 0) ? static_cast<double>(metrics.wpx) / vw : 1.0;
    const double scaleY = (vh != 0) ? static_cast<double>(metrics.hpx) / vh : 1.0;
    metrics.scale = std::min(scaleX, scaleY);

    metrics.tx = (metrics.wpx - vw * metrics.scale) / 2.0;
    metrics.ty = (metrics.hpx - vh * metrics.scale) / 2.0;

    return metrics;
}

//----------------------------------------------------------------------------
// LottieWriter
//----------------------------------------------------------------------------

std::string LottieWriter::WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name)
{
    int w = 0;
    int h = 0;
    std::vector<PageMetrics> metrics;
    metrics.reserve(pages.size());
    for (const LottiePage *page : pages) {
        const PageMetrics m = ComputePageMetrics(*page);
        w = std::max(w, m.wpx);
        h = std::max(h, m.hpx);
        metrics.push_back(m);
    }

    const int pageCount = static_cast<int>(pages.size());

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"v\":\"5.7.0\",\"fr\":30,\"ip\":0,\"op\":" << pageCount << ",\"w\":" << w << ",\"h\":" << h
        << ",\"nm\":\"" << EscapeJsonString(name) << "\",\"ddd\":0,\"assets\":[],\"markers\":[],\"layers\":[";

    for (int i = 0; i < pageCount; ++i) {
        if (i) out << ",";
        const LottiePage &page = *pages[i];
        const PageMetrics &m = metrics[i];

        const double px = m.tx + m.scale * page.originX;
        const double py = m.ty + m.scale * page.originY;
        const double s = m.scale * 100.0;

        out << "{\"ddd\":0,\"ind\":" << (i + 1) << ",\"ty\":4,\"nm\":\"page-" << (i + 1) << "\",\"sr\":1,"
            << "\"ks\":{\"o\":{\"a\":0,\"k\":100},\"r\":{\"a\":0,\"k\":0},"
            << "\"p\":{\"a\":0,\"k\":[" << FormatNumber(px) << "," << FormatNumber(py) << ",0]},"
            << "\"a\":{\"a\":0,\"k\":[0,0,0]},"
            << "\"s\":{\"a\":0,\"k\":[" << FormatNumber(s) << "," << FormatNumber(s) << ",100]}},"
            << "\"ao\":0,\"shapes\":[" << WriteLayerShapes(page) << "],"
            << "\"ip\":" << i << ",\"op\":" << (i + 1) << ",\"st\":0,\"bm\":0}";
    }

    out << "]}";
    return out.str();
}

} // namespace vrv
