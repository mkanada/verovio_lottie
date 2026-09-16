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

#include "csscolor.h"
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

// Escapes a UTF-8 string for a JSON string literal. Class names, colors and xml:ids are never
// anything but printable ASCII, but D01's common text runs come straight from the score's own
// text content (e.g. a MusicXML credit line) and can carry raw control characters such as an
// embedded '\n' - illegal unescaped inside a JSON string per spec - so every control character
// is escaped, not just '"'/'\\'.
static std::string EscapeJsonString(const std::string &s)
{
    static const char *const kHex = "0123456789abcdef";

    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(kHex[(c >> 4) & 0xF]);
                    out.push_back(kHex[c & 0xF]);
                }
                else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

// Colors in the IR are packed 24-bit integers (COLOR_BLACK, COLOR_WHITE, parsed #RRGGBB, etc.).
static void ColorIntToRgb01(int color, double &r, double &g, double &b)
{
    r = ((color >> 16) & 0xFF) / 255.0;
    g = ((color >> 8) & 0xFF) / 255.0;
    b = (color & 0xFF) / 255.0;
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
        else if (it->text) {
            // Common text runs (D01) are serialized as independent "ty":5 layers by
            // CollectTextRuns/WriteTextLayer, not as shapes inside this page's shape layer.
            continue;
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
// Common text (D01, docs/plano/D01-texto-comum.md)
//----------------------------------------------------------------------------

// A LottieTextRun with its inherited color and effective bold/italic already resolved, the
// same way ResolveColor's inheritedColor propagates down the tree for shapes.
namespace {
    struct ResolvedTextRun {
        const LottieTextRun *run;
        int color;
        bool bold;
        bool italic;
    };
} // namespace

// One of the four vendored Liberation Serif styles (B03 originally covered only
// Regular/Italic/Bold; Bold Italic added in D01-5 after a real bold+italic tempo submark
// - "49"/"50"/"51" fluctuation numbers in Clair de Lune, class "tempo" for the CSS bold rule
// plus an explicit font-style="italic" on the run - was confirmed rendering upright instead
// of slanted, see docs/plano/D01-5-bold-italico-tempo.md).
static std::string SelectFontStyleName(bool bold, bool italic)
{
    if (bold && italic) return "BoldItalic";
    if (bold) return "Bold";
    if (italic) return "Italic";
    return "Regular";
}

// SvgDeviceContext::Commit embeds a global CSS rule in every SVG output ("g.ending, g.fing,
// g.reh, g.tempo {font-weight:bold;} g.dir, g.dynam, g.mNum {font-style:italic;} g.label
// {font-weight:normal;}", svgdevicecontext.cpp) that some engraving code relies on instead of
// setting FontInfo's own style/weight (e.g. a numeric tempo submark comes through as plain
// italic FontInfo, visually bold only because of this class rule) - matched here the same way,
// by LottieNode::className (identical string to the SVG "class" attribute, see StartGraphic),
// so those runs come out visually the same as the SVG. Cumulative down the tree (mirrors CSS
// inheritance): "label" always wins over an ancestor's bold, since it is meant to cancel it.
namespace {
    struct TextStyleContext {
        bool bold = false;
        bool italic = false;
    };
} // namespace

static bool HasClassToken(const std::string &classNames, const char *token)
{
    std::istringstream iss(classNames);
    std::string word;
    while (iss >> word) {
        if (word == token) return true;
    }
    return false;
}

static TextStyleContext ApplyClassStyleRule(const std::string &classNames, TextStyleContext ctx)
{
    if (HasClassToken(classNames, "ending") || HasClassToken(classNames, "fing") || HasClassToken(classNames, "reh")
        || HasClassToken(classNames, "tempo")) {
        ctx.bold = true;
    }
    if (HasClassToken(classNames, "dir") || HasClassToken(classNames, "dynam") || HasClassToken(classNames, "mNum")) {
        ctx.italic = true;
    }
    if (HasClassToken(classNames, "label")) {
        ctx.bold = false;
    }
    return ctx;
}

static std::string LiberationFontName(const std::string &styleName)
{
    return "LiberationSerif-" + styleName;
}

static int MapJustification(data_HORIZONTALALIGNMENT alignment)
{
    switch (alignment) {
        case HORIZONTALALIGNMENT_center: return 2;
        case HORIZONTALALIGNMENT_right: return 1;
        default: return 0;
    }
}

// Walks the same tree AppendChildrenReversed does for shapes, resolving each text run's
// inherited color the way ResolveColor's inheritedColor already does, plus the CSS class rule
// above, and skipping hidden subtrees exactly like AppendChildrenReversed. Traversal order does
// not matter here: unlike shapes (composited within one "shapes" array, where paint order is
// significant), every run becomes its own independent Lottie layer.
static void CollectTextRuns(
    const LottieNode &node, int inheritedColor, TextStyleContext styleCtx, std::vector<ResolvedTextRun> &out)
{
    const int nodeColor = ResolveColor(node.colorCss, inheritedColor);
    styleCtx = ApplyClassStyleRule(node.className, styleCtx);
    for (const LottieChild &child : node.children) {
        if (child.group) {
            if (child.group->hidden) continue;
            CollectTextRuns(*child.group, nodeColor, styleCtx, out);
        }
        else if (child.text) {
            const int color = (child.text->color == COLOR_NONE) ? nodeColor : child.text->color;
            const bool bold = (child.text->weight == FONTWEIGHT_bold) || styleCtx.bold;
            const bool italic
                = (child.text->style == FONTSTYLE_italic) || (child.text->style == FONTSTYLE_oblique) || styleCtx.italic;
            out.push_back({ &(*child.text), color, bold, italic });
        }
    }
}

// A common-text layer ("ty":5), sibling of the page's own shape layer ("ty":4). px/py/sPercent
// are the exact same numbers already computed for that page's shape layer (see WriteAnimation's
// page loop) - the run's own (page-px) origin is folded in the same way page.originX/Y already
// is, so the run lands exactly where a shape vertex at that position would.
static std::string WriteTextLayer(
    const ResolvedTextRun &resolved, int ind, int parentInd, int ip, int op, double px, double py, double scale, double sPercent)
{
    const LottieTextRun &run = *resolved.run;
    const double layerX = px + scale * run.origin.x;
    const double layerY = py + scale * run.origin.y;

    double r, g, b;
    ColorIntToRgb01(resolved.color, r, g, b);

    const std::string fontName = LiberationFontName(SelectFontStyleName(resolved.bold, resolved.italic));

    std::ostringstream out;
    out << "{\"ddd\":0,\"ind\":" << ind << ",\"ty\":5,\"nm\":\"text\",\"sr\":1,";
    if (parentInd > 0) {
        out << "\"parent\":" << parentInd << ",";
    }
    out << "\"ks\":{\"o\":{\"a\":0,\"k\":100},\"r\":{\"a\":0,\"k\":0},"
        << "\"p\":{\"a\":0,\"k\":[" << FormatNumber(layerX) << "," << FormatNumber(layerY) << ",0]},"
        << "\"a\":{\"a\":0,\"k\":[0,0,0]},"
        << "\"s\":{\"a\":0,\"k\":[" << FormatNumber(sPercent) << "," << FormatNumber(sPercent) << ",100]}},"
        << "\"ao\":0,\"t\":{\"d\":{\"k\":[{\"s\":{\"s\":" << FormatNumber(run.pointSize) << ",\"f\":\""
        << EscapeJsonString(fontName) << "\",\"t\":\"" << EscapeJsonString(UTF32to8(run.text))
        << "\",\"j\":" << MapJustification(run.alignment) << ",\"tr\":" << FormatNumber(run.letterSpacing)
        << ",\"fc\":[" << FormatNumber(r) << "," << FormatNumber(g) << "," << FormatNumber(b)
        << "]},\"t\":0}]}},\"ip\":" << ip << ",\"op\":" << op << ",\"st\":0,\"bm\":0}";
    return out.str();
}

// Fixed fonts.list entries (B03: Regular/Italic/Bold always embedded together, whether or not
// the piece actually uses each style - keeps package size predictable, see D01's "Decisões de
// escopo"; BoldItalic added in D01-5, same always-embedded treatment), format confirmed
// against the dotlottie-rs/ThorVG engine (LottieFiles/dotlottie-rs, commit eb44c991,
// test/resources/resolver.json and src/renderer/thorvg.rs's
// asset_resolver_memoizes_loaded_fonts_and_failures test, both using "fName"/"fFamily"/
// "fStyle"/"fPath"/"origin":3).
static std::string WriteFontsList()
{
    static const char *const kStyles[] = { "Regular", "Italic", "Bold", "BoldItalic" };
    std::vector<std::string> items;
    for (const char *style : kStyles) {
        const std::string fName = LiberationFontName(style);
        items.push_back("{\"fName\":\"" + fName + "\",\"fFamily\":\"Liberation Serif\",\"fStyle\":\"" + style
            + "\",\"fPath\":\"f/" + fName + ".ttf\",\"origin\":3}");
    }
    return "\"fonts\":{\"list\":[" + JoinItems(items) + "]}";
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
    const std::unordered_set<std::string> &interactiveIds, const LottiePageTurnLayout &pageTurn, double peekFraction,
    bool embedCommonText)
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
    // pageTurn.enabled, so page layers start at 2 instead of 1 in that case. Common-text
    // layers (D01) are allocated after every camera/page "ind" already in use, in a single
    // counter running across all pages (not reset per page).
    const int cameraInd = 1;
    const int firstPageInd = pageTurn.enabled ? 2 : 1;
    int nextTextInd = firstPageInd + pageCount;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"v\":\"5.7.0\",\"fr\":30,\"ip\":0,\"op\":" << op << ",\"w\":" << w << ",\"h\":" << h << ",\"nm\":\""
        << EscapeJsonString(name) << "\",\"ddd\":0,\"assets\":[]";
    if (embedCommonText) {
        out << "," << WriteFontsList();
    }
    out << ",\"markers\":[" << JoinItems(markerItems) << "],\"layers\":[";

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

        if (embedCommonText) {
            std::vector<ResolvedTextRun> textRuns;
            CollectTextRuns(*page.root, ResolveColor(page.root->colorCss, COLOR_BLACK), TextStyleContext{}, textRuns);
            const int parentInd = pageTurn.enabled ? cameraInd : 0;
            for (const ResolvedTextRun &resolved : textRuns) {
                out << "," << WriteTextLayer(resolved, nextTextInd++, parentInd, pageIp, pageOp, px, py, m.scale, s);
            }
        }
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
