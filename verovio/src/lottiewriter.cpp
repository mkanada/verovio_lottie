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
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

//----------------------------------------------------------------------------

#include "vrv.h"

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

// rgb(r,g,b), integer components 0-255 (out-of-range values are clamped, as CSS requires).
static bool ParseRgbFunction(const std::string &lowerCss, int &outColor)
{
    if ((lowerCss.compare(0, 4, "rgb(") != 0) || (lowerCss.back() != ')')) {
        return false;
    }

    const std::string inner = lowerCss.substr(4, lowerCss.size() - 5);
    std::vector<int> components;
    std::stringstream ss(inner);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            std::size_t consumed = 0;
            components.push_back(std::stoi(token, &consumed));
        }
        catch (const std::exception &) {
            return false;
        }
    }
    if (components.size() != 3) {
        return false;
    }

    auto clamp = [](int v) { return std::max(0, std::min(255, v)); };
    outColor = (clamp(components[0]) << 16) | (clamp(components[1]) << 8) | clamp(components[2]);
    return true;
}

// A practical subset of the CSS named colors, not the full CSS spec list.
static bool ParseNamedColor(const std::string &lowerCss, int &outColor)
{
    static const std::map<std::string, int> namedColors = {
        { "black", 0x000000 },
        { "white", 0xFFFFFF },
        { "red", 0xFF0000 },
        { "green", 0x008000 },
        { "blue", 0x0000FF },
        { "gray", 0x808080 },
        { "grey", 0x808080 },
        { "silver", 0xC0C0C0 },
        { "maroon", 0x800000 },
        { "purple", 0x800080 },
        { "fuchsia", 0xFF00FF },
        { "lime", 0x00FF00 },
        { "olive", 0x808000 },
        { "yellow", 0xFFFF00 },
        { "navy", 0x000080 },
        { "teal", 0x008080 },
        { "aqua", 0x00FFFF },
        { "orange", 0xFFA500 },
    };

    const auto it = namedColors.find(lowerCss);
    if (it == namedColors.end()) {
        return false;
    }
    outColor = it->second;
    return true;
}

// Resolves an inline @color value (from LottieNode::colorCss) into a packed 24-bit color.
// Supports "#RGB", "#RRGGBB", "rgb(r,g,b)" and a subset of CSS named colors. An unparseable,
// non-empty value is logged once and defaults to black, matching the SVG behavior of falling
// back to the initial "black" fill/stroke rather than silently inheriting.
static int ResolveColor(const std::string &colorCss, int inheritedColor)
{
    if (colorCss.empty()) {
        return inheritedColor;
    }

    const std::size_t begin = colorCss.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return inheritedColor;
    }
    const std::size_t end = colorCss.find_last_not_of(" \t\r\n");
    const std::string trimmed = colorCss.substr(begin, end - begin + 1);

    int parsed = COLOR_NONE;
    if (ParseHexColor(trimmed, parsed)) {
        return parsed;
    }

    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });

    if (ParseRgbFunction(lower, parsed)) {
        return parsed;
    }
    if (ParseNamedColor(lower, parsed)) {
        return parsed;
    }

    static std::set<std::string> warnedColors;
    if (warnedColors.insert(trimmed).second) {
        LogWarning("LottieWriter: unsupported CSS color value '%s'; defaulting to black.", trimmed.c_str());
    }
    return COLOR_BLACK;
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

// A note's (or note group's) reserved, non-overlapping frame slot within the shared "score"
// timeline (see LottieHighlightGroup) - resolved to the shapes it applies to at write time,
// the same way ResolveColor()'s inheritedColor already propagates down the tree.
namespace {
    struct ActiveHighlight {
        int startFrame = 0;
        int durationFrames = 0;
    };
} // namespace

// Keyframed "k" array of a color property: holds `baseColor` from t=0 (only needed if the
// group doesn't already start at frame 0), jumps to `highlightColor` at startFrame, and
// fades linearly back to `baseColor` by startFrame+durationFrames. Same shape as
// color_property() in compare/fixtures/b01/gen.py, validated empirically against
// dotlottie-rs by the B01 spike.
static std::string WriteColorKeyframes(int baseColor, int highlightColor, const ActiveHighlight &highlight)
{
    double br, bg, bb, hr, hg, hb;
    ColorIntToRgb01(baseColor, br, bg, bb);
    ColorIntToRgb01(highlightColor, hr, hg, hb);
    const int endFrame = highlight.startFrame + highlight.durationFrames;

    std::ostringstream out;
    out << "[";
    if (highlight.startFrame > 0) {
        out << "{\"t\":0,\"s\":[" << FormatNumber(br) << "," << FormatNumber(bg) << "," << FormatNumber(bb)
            << ",1],\"h\":1,\"i\":{\"x\":1,\"y\":1},\"o\":{\"x\":0,\"y\":0}},";
    }
    out << "{\"t\":" << highlight.startFrame << ",\"s\":[" << FormatNumber(hr) << "," << FormatNumber(hg) << ","
        << FormatNumber(hb) << ",1],\"i\":{\"x\":1,\"y\":1},\"o\":{\"x\":0,\"y\":0}},";
    out << "{\"t\":" << endFrame << ",\"s\":[" << FormatNumber(br) << "," << FormatNumber(bg) << ","
        << FormatNumber(bb) << ",1]}";
    out << "]";
    return out.str();
}

// M3 (see docs/plano/C03-slots-interativos.md): "sid" tags a color property so the host can
// override it directly with Player::set_color_slot, independent of whatever the M2 (highlight)
// keyframes/branch above are doing - a slot always wins over the native "k" value regardless of
// the current frame (confirmed empirically by B01's E6). Orthogonal to `highlight`: emitted (or
// not) the same way on both the "a":1 and "a":0 branches.
static std::string WriteColorObject(
    int color, const ActiveHighlight *highlight, int highlightColor, const std::string *slotId)
{
    std::ostringstream out;
    if (highlight) {
        out << "{\"a\":1,";
        if (slotId) out << "\"sid\":\"" << EscapeJsonString(*slotId) << "\",";
        out << "\"k\":" << WriteColorKeyframes(color, highlightColor, *highlight) << "}";
    }
    else {
        double r, g, b;
        ColorIntToRgb01(color, r, g, b);
        out << "{\"a\":0,";
        if (slotId) out << "\"sid\":\"" << EscapeJsonString(*slotId) << "\",";
        out << "\"k\":[" << FormatNumber(r) << "," << FormatNumber(g) << "," << FormatNumber(b) << ",1]}";
    }
    return out.str();
}

static std::string WriteFill(const LottieShape &shape, int inheritedColor, const ActiveHighlight *highlight,
    int highlightColor, const std::string *slotId)
{
    const int color = (shape.fillColor == COLOR_NONE) ? inheritedColor : shape.fillColor;

    std::ostringstream out;
    out << "{\"ty\":\"fl\",\"c\":" << WriteColorObject(color, highlight, highlightColor, slotId);
    out << ",\"o\":{\"a\":0,\"k\":" << FormatNumber(shape.fillOpacity * 100) << "},\"r\":1}";
    return out.str();
}

static std::string WriteStroke(const LottieShape &shape, int inheritedColor, const ActiveHighlight *highlight,
    int highlightColor, const std::string *slotId)
{
    const int color = (shape.strokeColor == COLOR_NONE) ? inheritedColor : shape.strokeColor;

    std::ostringstream out;
    out << "{\"ty\":\"st\",\"c\":" << WriteColorObject(color, highlight, highlightColor, slotId);
    out << ",\"o\":{\"a\":0,\"k\":" << FormatNumber(shape.strokeOpacity * 100)
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

static std::string WriteShapeGroup(const LottieShape &shape, int inheritedColor, const ActiveHighlight *highlight,
    int highlightColor, const std::string *slotId)
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
        items.push_back(WriteStroke(shape, inheritedColor, highlight, highlightColor, slotId));
    }
    if (shape.hasFill) {
        items.push_back(WriteFill(shape, inheritedColor, highlight, highlightColor, slotId));
    }
    items.push_back(WriteTransformDefault());

    return "{\"ty\":\"gr\",\"it\":[" + JoinItems(items) + "]}";
}

// Camera "p" keyframe array (C04, docs/plano/C04-paginas-virada.md): one hold stop per page's
// resting x plus, for every boundary that actually got both event ids (see BuildLayout), two
// more stops carrying the "peek" (partial move, held) and "cover" (completes the move) phases.
// Same keyframe shape/easing handles as WriteColorKeyframes, generalized to more than 3 stops
// and sorted by frame (boundaries are not necessarily contiguous with their page index when
// some were skipped - see LottiePageBoundary::fromPage).
static std::string WriteCameraKeyframes(const LottiePageTurnLayout &pageTurn, int trackStep, double peekFraction)
{
    struct Stop {
        int frame;
        double x;
    };
    std::vector<Stop> stops;

    auto restX = [trackStep](std::size_t pageIndex) { return -(static_cast<double>(pageIndex) * trackStep); };

    for (std::size_t i = 0; i < pageTurn.pageRestFrames.size(); ++i) {
        stops.push_back({ pageTurn.pageRestFrames[i], restX(i) });
    }
    for (const LottiePageBoundary &boundary : pageTurn.boundaries) {
        const double fromX = restX(static_cast<std::size_t>(boundary.fromPage));
        const double peekX = fromX - peekFraction * trackStep;
        stops.push_back({ boundary.peekStartFrame, fromX });
        stops.push_back({ boundary.peekStartFrame + boundary.peekDurationFrames, peekX });
        stops.push_back({ boundary.coverStartFrame, peekX });
        stops.push_back(
            { boundary.coverStartFrame + boundary.coverDurationFrames, restX(static_cast<std::size_t>(boundary.fromPage) + 1) });
    }

    std::stable_sort(stops.begin(), stops.end(), [](const Stop &a, const Stop &b) { return a.frame < b.frame; });

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < stops.size(); ++i) {
        if (i) out << ",";
        out << "{\"t\":" << stops[i].frame << ",\"s\":[" << FormatNumber(stops[i].x) << ",0,0]";
        if (i + 1 < stops.size()) {
            out << ",\"i\":{\"x\":1,\"y\":1},\"o\":{\"x\":0,\"y\":0}";
        }
        out << "}";
    }
    out << "]";
    return out.str();
}

using HighlightsById = std::unordered_map<std::string, ActiveHighlight>;
using InteractiveIds = std::unordered_set<std::string>;

static std::string WriteNodeGroup(const LottieNode &node, int inheritedColor, const ActiveHighlight *highlight,
    int highlightColor, const HighlightsById &highlightsById, const std::string *slotId,
    const InteractiveIds &interactiveIds);

// Children are written from last to first: in the SVG/IR document order the later sibling
// paints on top, while in Lottie the first item of "it" paints on top.
static void AppendChildrenReversed(const std::vector<LottieChild> &children, int inheritedColor,
    const ActiveHighlight *highlight, int highlightColor, const HighlightsById &highlightsById,
    const std::string *slotId, const InteractiveIds &interactiveIds, std::vector<std::string> &items)
{
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (it->group) {
            if (it->group->hidden) {
                continue;
            }
            items.push_back(WriteNodeGroup(
                *it->group, inheritedColor, highlight, highlightColor, highlightsById, slotId, interactiveIds));
        }
        else {
            items.push_back(WriteShapeGroup(it->shape, inheritedColor, highlight, highlightColor, slotId));
        }
    }
}

static std::string WriteNodeGroup(const LottieNode &node, int inheritedColor, const ActiveHighlight *highlight,
    int highlightColor, const HighlightsById &highlightsById, const std::string *slotId,
    const InteractiveIds &interactiveIds)
{
    const int nodeColor = ResolveColor(node.colorCss, inheritedColor);

    ActiveHighlight ownHighlight;
    const ActiveHighlight *nodeHighlight = highlight;
    const std::string *nodeSlotId = slotId;
    if (!node.id.empty()) {
        const auto it = highlightsById.find(node.id);
        if (it != highlightsById.end()) {
            ownHighlight = it->second;
            nodeHighlight = &ownHighlight;
        }
        if (interactiveIds.find(node.id) != interactiveIds.end()) {
            nodeSlotId = &node.id;
        }
    }

    std::vector<std::string> items;
    AppendChildrenReversed(
        node.children, nodeColor, nodeHighlight, highlightColor, highlightsById, nodeSlotId, interactiveIds, items);
    items.push_back(node.hasRotation ? WriteTransformWithRotation(node.rotationOrigin, node.rotation)
                                      : WriteTransformDefault());

    const std::string &nm = !node.id.empty() ? node.id : node.className;

    return "{\"ty\":\"gr\",\"nm\":\"" + EscapeJsonString(nm) + "\",\"mn\":\"" + EscapeJsonString(node.className)
        + "\",\"it\":[" + JoinItems(items) + "]}";
}

static std::string WriteLayerShapes(const LottiePage &page, const HighlightsById &highlightsById, int highlightColor,
    const InteractiveIds &interactiveIds)
{
    std::vector<std::string> items;
    const int rootColor = ResolveColor(page.root->colorCss, COLOR_BLACK);
    AppendChildrenReversed(
        page.root->children, rootColor, nullptr, highlightColor, highlightsById, nullptr, interactiveIds, items);
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

std::string LottieWriter::WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name,
    const std::vector<LottieHighlightGroup> &highlightGroups, int highlightColor,
    const std::unordered_set<std::string> &interactiveIds, const LottiePageTurnLayout &pageTurn, double peekFraction)
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
    // The horizontal track step between adjacent pages (C04): reuses the composition's own
    // width, which is also the viewport the camera clips to (see WriteCameraKeyframes / the
    // Fit::Contain argument in C04's plan doc for why content outside it never renders).
    const int trackStep = w;

    HighlightsById highlightsById;
    int highlightEnd = 0;
    for (const LottieHighlightGroup &group : highlightGroups) {
        highlightEnd = std::max(highlightEnd, group.startFrame + group.durationFrames);
        for (const std::string &memberId : group.memberIds) {
            highlightsById[memberId] = { group.startFrame, group.durationFrames };
        }
    }
    int op = std::max(pageCount, highlightEnd);
    if (pageTurn.enabled) {
        op = std::max(op, pageTurn.endFrame);
    }

    std::vector<std::string> markerItems;
    if (!highlightGroups.empty()) {
        markerItems.push_back("{\"cm\":\"idle\",\"tm\":0,\"dr\":1}");
        for (const LottieHighlightGroup &group : highlightGroups) {
            markerItems.push_back("{\"cm\":\"" + EscapeJsonString(group.name) + "\",\"tm\":"
                + std::to_string(group.startFrame) + ",\"dr\":" + std::to_string(group.durationFrames) + "}");
        }
    }
    if (pageTurn.enabled) {
        for (std::size_t i = 0; i < pageTurn.pageMarkers.size(); ++i) {
            markerItems.push_back("{\"cm\":\"" + EscapeJsonString(pageTurn.pageMarkers[i]) + "\",\"tm\":"
                + std::to_string(pageTurn.pageRestFrames[i]) + ",\"dr\":1}");
        }
        for (const LottiePageBoundary &boundary : pageTurn.boundaries) {
            markerItems.push_back("{\"cm\":\"" + EscapeJsonString(boundary.peekMarker) + "\",\"tm\":"
                + std::to_string(boundary.peekStartFrame)
                + ",\"dr\":" + std::to_string(boundary.peekDurationFrames) + "}");
            markerItems.push_back("{\"cm\":\"" + EscapeJsonString(boundary.coverMarker) + "\",\"tm\":"
                + std::to_string(boundary.coverStartFrame)
                + ",\"dr\":" + std::to_string(boundary.coverDurationFrames) + "}");
        }
    }

    // Reserved layer indices for the camera rig (C04): "ind" 1 is the camera itself when
    // pageTurn.enabled, so page layers start at 2 instead of 1 in that case.
    const int cameraInd = 1;
    const int firstPageInd = pageTurn.enabled ? 2 : 1;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"v\":\"5.7.0\",\"fr\":30,\"ip\":0,\"op\":" << op << ",\"w\":" << w << ",\"h\":" << h << ",\"nm\":\""
        << EscapeJsonString(name) << "\",\"ddd\":0,\"assets\":[],\"markers\":[" << JoinItems(markerItems)
        << "],\"layers\":[";

    bool needComma = false;
    if (pageTurn.enabled) {
        out << "{\"ddd\":0,\"ind\":" << cameraInd << ",\"ty\":3,\"nm\":\"camera\",\"sr\":1,"
            << "\"ks\":{\"o\":{\"a\":0,\"k\":100},\"r\":{\"a\":0,\"k\":0},"
            << "\"p\":{\"a\":1,\"k\":" << WriteCameraKeyframes(pageTurn, trackStep, peekFraction) << "},"
            << "\"a\":{\"a\":0,\"k\":[0,0,0]},\"s\":{\"a\":0,\"k\":[100,100,100]}},"
            << "\"ao\":0,\"ip\":0,\"op\":" << op << ",\"st\":0,\"bm\":0}";
        needComma = true;
    }

    for (int i = 0; i < pageCount; ++i) {
        if (needComma) out << ",";
        needComma = true;
        const LottiePage &page = *pages[i];
        const PageMetrics &m = metrics[i];

        double px = m.tx + m.scale * page.originX;
        if (pageTurn.enabled) {
            px += static_cast<double>(i) * trackStep;
        }
        const double py = m.ty + m.scale * page.originY;
        const double s = m.scale * 100.0;

        int pageIp;
        int pageOp;
        if (pageTurn.enabled) {
            // Visibility is now purely the camera + the composition's own viewport clipping
            // (C04) - every page layer is always "on".
            pageIp = 0;
            pageOp = op;
        }
        else {
            // Pre-C04 windowing: only the last page's own out-point is stretched to cover the
            // highlight tail (C02's "Decisão de escopo" - this branch only ever sees a single
            // highlighted page; the multi-page case always has pageTurn.enabled).
            pageIp = i;
            pageOp = (i == pageCount - 1) ? op : (i + 1);
        }

        out << "{\"ddd\":0,\"ind\":" << (firstPageInd + i) << ",\"ty\":4,\"nm\":\"page-" << (i + 1) << "\",\"sr\":1,";
        if (pageTurn.enabled) {
            out << "\"parent\":" << cameraInd << ",";
        }
        out << "\"ks\":{\"o\":{\"a\":0,\"k\":100},\"r\":{\"a\":0,\"k\":0},"
            << "\"p\":{\"a\":0,\"k\":[" << FormatNumber(px) << "," << FormatNumber(py) << ",0]},"
            << "\"a\":{\"a\":0,\"k\":[0,0,0]},"
            << "\"s\":{\"a\":0,\"k\":[" << FormatNumber(s) << "," << FormatNumber(s) << ",100]}},"
            << "\"ao\":0,\"shapes\":[" << WriteLayerShapes(page, highlightsById, highlightColor, interactiveIds)
            << "],"
            << "\"ip\":" << pageIp << ",\"op\":" << pageOp << ",\"st\":0,\"bm\":0}";
    }

    out << "]}";
    return out.str();
}

//----------------------------------------------------------------------------
// LottieWriter::WriteStateMachine
//----------------------------------------------------------------------------

static std::string WriteSMTransition(const LottieSMTransition &transition)
{
    return "{\"type\":\"Transition\",\"toState\":\"" + EscapeJsonString(transition.toState)
        + "\",\"guards\":[{\"type\":\"Event\",\"inputName\":\"" + EscapeJsonString(transition.eventInput) + "\"}]}";
}

static std::string WriteSMState(const LottieSMState &state)
{
    std::vector<std::string> transitionItems;
    for (const LottieSMTransition &transition : state.transitions) {
        transitionItems.push_back(WriteSMTransition(transition));
    }
    const std::string transitions = "\"transitions\":[" + JoinItems(transitionItems) + "]";

    if (state.isGlobal) {
        // No "animation"/"segment"/"autoplay"/"loop": not part of the format actually
        // exercised by the B01 spike for GlobalState (compare/fixtures/b01/gen.py).
        return "{\"name\":\"" + EscapeJsonString(state.name) + "\",\"type\":\"GlobalState\"," + transitions + "}";
    }

    std::ostringstream out;
    out << "{\"name\":\"" << EscapeJsonString(state.name) << "\",\"type\":\"PlaybackState\",\"animation\":\""
        << EscapeJsonString(state.animation) << "\",\"segment\":\"" << EscapeJsonString(state.segment)
        << "\",\"autoplay\":" << (state.autoplay ? "true" : "false") << ",\"loop\":" << (state.loop ? "true" : "false")
        << "," << transitions << "}";
    return out.str();
}

// Inputs are derived from the transitions rather than requested from the caller, so that a
// state machine can never reference an Event input that was not declared (and vice versa).
static std::vector<std::string> CollectEventInputs(const LottieStateMachine &stateMachine)
{
    std::vector<std::string> inputs;
    std::unordered_set<std::string> seen;
    for (const LottieSMState &state : stateMachine.states) {
        for (const LottieSMTransition &transition : state.transitions) {
            if (seen.insert(transition.eventInput).second) {
                inputs.push_back(transition.eventInput);
            }
        }
    }
    return inputs;
}

std::string LottieWriter::WriteStateMachine(const LottieStateMachine &stateMachine)
{
    std::vector<std::string> stateItems;
    for (const LottieSMState &state : stateMachine.states) {
        stateItems.push_back(WriteSMState(state));
    }

    std::vector<std::string> inputItems;
    for (const std::string &name : CollectEventInputs(stateMachine)) {
        inputItems.push_back("{\"type\":\"Event\",\"name\":\"" + EscapeJsonString(name) + "\"}");
    }

    return "{\"initial\":\"" + EscapeJsonString(stateMachine.initial) + "\",\"states\":[" + JoinItems(stateItems)
        + "],\"inputs\":[" + JoinItems(inputItems) + "],\"interactions\":[]}";
}

//----------------------------------------------------------------------------
// LottieWriter::WriteManifest
//----------------------------------------------------------------------------

std::string LottieWriter::WriteManifest(const std::string &generator, const std::vector<std::string> &animationIds,
    const std::string &initialAnimation, const std::vector<std::string> &stateMachineIds,
    const std::string &initialStateMachine)
{
    std::vector<std::string> animationItems;
    for (const std::string &id : animationIds) {
        animationItems.push_back("{\"id\":\"" + EscapeJsonString(id) + "\"}");
    }

    std::ostringstream out;
    out << "{\"version\":\"2\",\"generator\":\"" << EscapeJsonString(generator) << "\",\"animations\":["
        << JoinItems(animationItems) << "]";

    if (!stateMachineIds.empty()) {
        std::vector<std::string> smItems;
        for (const std::string &id : stateMachineIds) {
            smItems.push_back("{\"id\":\"" + EscapeJsonString(id) + "\"}");
        }
        out << ",\"stateMachines\":[" << JoinItems(smItems) << "]";
    }

    out << ",\"initial\":{\"animation\":\"" << EscapeJsonString(initialAnimation) << "\"";
    if (!initialStateMachine.empty()) {
        out << ",\"stateMachine\":\"" << EscapeJsonString(initialStateMachine) << "\"";
    }
    out << "}}";

    return out.str();
}

} // namespace vrv
