/////////////////////////////////////////////////////////////////////////////
// Name:        svgpathparser.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "svgpathparser.h"

//----------------------------------------------------------------------------

#include <cctype>
#include <cmath>
#include <cstdlib>

//----------------------------------------------------------------------------

#include "pugixml.hpp"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// Local helpers
//----------------------------------------------------------------------------

namespace {

    //------------------------------------------------------------------
    // Number tokenizer
    //
    // SVG path data has no mandatory separator between numbers ("-54 0 -97" as well as the
    // sign-delimited "10-5" or the dot-delimited "1.5.5"), so numbers are read one at a time
    // straight off the string rather than split on whitespace first.
    //------------------------------------------------------------------

    void SkipSeparators(const std::string &d, size_t &pos)
    {
        while (pos < d.size() && (std::isspace(static_cast<unsigned char>(d[pos])) || d[pos] == ',')) {
            ++pos;
        }
    }

    bool ReadNumber(const std::string &d, size_t &pos, double &value)
    {
        SkipSeparators(d, pos);
        const size_t start = pos;
        size_t p = pos;
        if (p < d.size() && (d[p] == '+' || d[p] == '-')) ++p;
        bool hasDigits = false;
        while (p < d.size() && std::isdigit(static_cast<unsigned char>(d[p]))) {
            ++p;
            hasDigits = true;
        }
        if (p < d.size() && d[p] == '.') {
            ++p;
            while (p < d.size() && std::isdigit(static_cast<unsigned char>(d[p]))) {
                ++p;
                hasDigits = true;
            }
        }
        if (!hasDigits) return false;
        if (p < d.size() && (d[p] == 'e' || d[p] == 'E')) {
            size_t q = p + 1;
            if (q < d.size() && (d[q] == '+' || d[q] == '-')) ++q;
            if (q < d.size() && std::isdigit(static_cast<unsigned char>(d[q]))) {
                ++q;
                while (q < d.size() && std::isdigit(static_cast<unsigned char>(d[q]))) ++q;
                p = q;
            }
        }
        value = std::strtod(d.substr(start, p - start).c_str(), NULL);
        pos = p;
        return true;
    }

    bool ReadNumbers(const std::string &d, size_t &pos, int count, std::vector<double> &values)
    {
        values.clear();
        for (int i = 0; i < count; ++i) {
            double value;
            if (!ReadNumber(d, pos, value)) return false;
            values.push_back(value);
        }
        return true;
    }

    //------------------------------------------------------------------
    // Vector arithmetic
    //------------------------------------------------------------------

    LottieVec MakeVec(double x, double y) { return LottieVec{ x, y }; }

    LottieVec AddVec(const LottieVec &a, const LottieVec &b) { return LottieVec{ a.x + b.x, a.y + b.y }; }

    LottieVec SubVec(const LottieVec &a, const LottieVec &b) { return LottieVec{ a.x - b.x, a.y - b.y }; }

    LottieVec ScaleVec(const LottieVec &v, double factor) { return LottieVec{ v.x * factor, v.y * factor }; }

    // Reflection of `point` through `pivot` (used for S/T's implicit control point).
    LottieVec Reflect(const LottieVec &pivot, const LottieVec &point)
    {
        return LottieVec{ 2 * pivot.x - point.x, 2 * pivot.y - point.y };
    }

    bool NearlyEqual(const LottieVec &a, const LottieVec &b)
    {
        static const double epsilon = 1e-6;
        return (std::abs(a.x - b.x) < epsilon) && (std::abs(a.y - b.y) < epsilon);
    }

    //------------------------------------------------------------------
    // PathBuilder: turns one "d" attribute into vrv::LottieBezier subpaths
    //------------------------------------------------------------------

    class PathBuilder {
    public:
        PathBuilder(std::vector<LottieBezier> &paths) : m_paths(paths) {}

        bool Parse(const std::string &d)
        {
            size_t pos = 0;
            char cmd = '\0';
            while (true) {
                SkipSeparators(d, pos);
                if (pos >= d.size()) break;
                const char c = d[pos];
                if (std::isalpha(static_cast<unsigned char>(c))) {
                    cmd = c;
                    ++pos;
                }
                else if (cmd == '\0') {
                    LogWarning("ParseSvgPathData: number found before any command");
                    return false;
                }
                else if (std::toupper(static_cast<unsigned char>(cmd)) == 'Z') {
                    // 'Z'/'z' take no arguments, so a bare number here cannot be an implicit
                    // repeat of it; the data is malformed.
                    LogWarning("ParseSvgPathData: unexpected number after 'Z'");
                    return false;
                }
                else if (cmd == 'M') {
                    cmd = 'L'; // extra coordinate pairs after a moveto are implicit linetos
                }
                else if (cmd == 'm') {
                    cmd = 'l';
                }
                if (!this->DoCommand(d, pos, cmd)) return false;
            }
            this->FinalizeSubpath(false);
            return true;
        }

    private:
        enum class LastCtrl { None, Cubic, Quad };

        bool DoCommand(const std::string &d, size_t &pos, char cmd)
        {
            const bool relative = (std::islower(static_cast<unsigned char>(cmd)) != 0);
            // S/T reflect the control point of an *immediately preceding* C/S or Q/T; read that
            // state before resetting it below (every other command breaks the chain).
            const LastCtrl previousLastCtrlType = m_lastCtrlType;
            const LottieVec previousLastCtrl = m_lastCtrl;
            m_lastCtrlType = LastCtrl::None;

            std::vector<double> args;
            switch (std::toupper(static_cast<unsigned char>(cmd))) {
                case 'M': {
                    if (!ReadNumbers(d, pos, 2, args)) return false;
                    const LottieVec p
                        = relative ? AddVec(m_current, MakeVec(args[0], args[1])) : MakeVec(args[0], args[1]);
                    this->StartSubpath(p);
                    return true;
                }
                case 'L': {
                    if (!ReadNumbers(d, pos, 2, args)) return false;
                    const LottieVec p
                        = relative ? AddVec(m_current, MakeVec(args[0], args[1])) : MakeVec(args[0], args[1]);
                    this->AddLineVertex(p);
                    return true;
                }
                case 'H': {
                    if (!ReadNumbers(d, pos, 1, args)) return false;
                    const double x = relative ? m_current.x + args[0] : args[0];
                    this->AddLineVertex(MakeVec(x, m_current.y));
                    return true;
                }
                case 'V': {
                    if (!ReadNumbers(d, pos, 1, args)) return false;
                    const double y = relative ? m_current.y + args[0] : args[0];
                    this->AddLineVertex(MakeVec(m_current.x, y));
                    return true;
                }
                case 'C': {
                    if (!ReadNumbers(d, pos, 6, args)) return false;
                    const LottieVec base = relative ? m_current : LottieVec();
                    const LottieVec c1 = AddVec(base, MakeVec(args[0], args[1]));
                    const LottieVec c2 = AddVec(base, MakeVec(args[2], args[3]));
                    const LottieVec end = AddVec(base, MakeVec(args[4], args[5]));
                    this->AddCubicVertex(c1, c2, end);
                    m_lastCtrlType = LastCtrl::Cubic;
                    m_lastCtrl = c2;
                    return true;
                }
                case 'S': {
                    if (!ReadNumbers(d, pos, 4, args)) return false;
                    const LottieVec base = relative ? m_current : LottieVec();
                    const LottieVec c1
                        = (previousLastCtrlType == LastCtrl::Cubic) ? Reflect(m_current, previousLastCtrl) : m_current;
                    const LottieVec c2 = AddVec(base, MakeVec(args[0], args[1]));
                    const LottieVec end = AddVec(base, MakeVec(args[2], args[3]));
                    this->AddCubicVertex(c1, c2, end);
                    m_lastCtrlType = LastCtrl::Cubic;
                    m_lastCtrl = c2;
                    return true;
                }
                case 'Q': {
                    if (!ReadNumbers(d, pos, 4, args)) return false;
                    const LottieVec base = relative ? m_current : LottieVec();
                    const LottieVec qc = AddVec(base, MakeVec(args[0], args[1]));
                    const LottieVec end = AddVec(base, MakeVec(args[2], args[3]));
                    // Degree-elevated to a cubic, as in LottieDeviceContext::DrawQuadBezierPath.
                    const LottieVec c1 = AddVec(m_current, ScaleVec(SubVec(qc, m_current), 2.0 / 3.0));
                    const LottieVec c2 = AddVec(end, ScaleVec(SubVec(qc, end), 2.0 / 3.0));
                    this->AddCubicVertex(c1, c2, end);
                    m_lastCtrlType = LastCtrl::Quad;
                    m_lastCtrl = qc;
                    return true;
                }
                case 'T': {
                    if (!ReadNumbers(d, pos, 2, args)) return false;
                    const LottieVec end
                        = relative ? AddVec(m_current, MakeVec(args[0], args[1])) : MakeVec(args[0], args[1]);
                    const LottieVec qc
                        = (previousLastCtrlType == LastCtrl::Quad) ? Reflect(m_current, previousLastCtrl) : m_current;
                    const LottieVec c1 = AddVec(m_current, ScaleVec(SubVec(qc, m_current), 2.0 / 3.0));
                    const LottieVec c2 = AddVec(end, ScaleVec(SubVec(qc, end), 2.0 / 3.0));
                    this->AddCubicVertex(c1, c2, end);
                    m_lastCtrlType = LastCtrl::Quad;
                    m_lastCtrl = qc;
                    return true;
                }
                case 'Z': {
                    this->FinalizeSubpath(true);
                    return true;
                }
                case 'A': {
                    // rx ry x-axis-rotation large-arc-flag sweep-flag x y
                    if (!ReadNumbers(d, pos, 7, args)) {
                        LogWarning("ParseSvgPathData: malformed elliptical arc command, aborting");
                        return false;
                    }
                    // Not used by Verovio's own font data (Leipzig/Bravura only emit
                    // M c h l s v z); rather than implement arc-to-bezier conversion for a
                    // segment that never occurs, ignore it entirely as the plan specifies,
                    // including not advancing the current point.
                    LogWarning("ParseSvgPathData: elliptical arc command is not supported, ignoring segment");
                    return true;
                }
                default:
                    LogWarning("ParseSvgPathData: unsupported command '%c'", cmd);
                    return false;
            }
        }

        void StartSubpath(const LottieVec &p)
        {
            this->FinalizeSubpath(false);
            LottieBezier bezier;
            bezier.v.push_back(p);
            bezier.i.push_back(LottieVec());
            bezier.o.push_back(LottieVec());
            m_paths.push_back(std::move(bezier));
            m_hasOpenSubpath = true;
            m_current = p;
            m_subpathStart = p;
        }

        void AddLineVertex(const LottieVec &p)
        {
            if (!m_hasOpenSubpath) this->StartSubpath(m_current);
            LottieBezier &subpath = m_paths.back();
            subpath.v.push_back(p);
            subpath.i.push_back(LottieVec());
            subpath.o.push_back(LottieVec());
            m_current = p;
        }

        void AddCubicVertex(const LottieVec &c1, const LottieVec &c2, const LottieVec &end)
        {
            if (!m_hasOpenSubpath) this->StartSubpath(m_current);
            LottieBezier &subpath = m_paths.back();
            subpath.o.back() = SubVec(c1, subpath.v.back());
            subpath.v.push_back(end);
            subpath.i.push_back(SubVec(c2, end));
            subpath.o.push_back(LottieVec());
            m_current = end;
        }

        // asClosed == true only for an explicit 'Z'; called with false at EOF and before
        // starting the next subpath, when it is a no-op unless a subpath is still open.
        void FinalizeSubpath(bool asClosed)
        {
            if (!m_hasOpenSubpath) return;
            LottieBezier &subpath = m_paths.back();
            if (asClosed) {
                subpath.closed = true;
                // A Lottie shape closes on its own; keeping a last vertex that coincides with
                // the first would add a zero-length closing segment on top of it.
                if (subpath.v.size() > 1 && NearlyEqual(subpath.v.back(), subpath.v.front())) {
                    subpath.i[0] = subpath.i.back();
                    subpath.v.pop_back();
                    subpath.i.pop_back();
                    subpath.o.pop_back();
                }
                m_current = m_subpathStart;
            }
            m_hasOpenSubpath = false;
        }

        std::vector<LottieBezier> &m_paths;
        bool m_hasOpenSubpath = false;
        LottieVec m_current;
        LottieVec m_subpathStart;
        LastCtrl m_lastCtrlType = LastCtrl::None;
        LottieVec m_lastCtrl;
    };

    //------------------------------------------------------------------
    // Glyph XML (transform + descendant <path> collection)
    //------------------------------------------------------------------

    void CollectPathNodes(const pugi::xml_node &node, std::vector<pugi::xml_node> &result)
    {
        for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
            if (std::string(child.name()) == "path") {
                result.push_back(child);
            }
            CollectPathNodes(child, result);
        }
    }

    // Only "scale(sx,sy)" (the sole form emitted by Leipzig/Bravura) is understood; a bare
    // "scale(s)" is also accepted since it costs nothing extra.
    bool ParseScaleTransform(const std::string &transform, double &sx, double &sy)
    {
        size_t begin = 0;
        SkipSeparators(transform, begin);
        if (transform.compare(begin, 5, "scale") != 0) return false;
        const size_t open = transform.find('(', begin);
        const size_t close = transform.find(')', open == std::string::npos ? 0 : open);
        if (open == std::string::npos || close == std::string::npos || close < open) return false;

        const std::string inner = transform.substr(open + 1, close - open - 1);
        size_t pos = 0;
        double a;
        if (!ReadNumber(inner, pos, a)) return false;
        SkipSeparators(inner, pos);
        double b;
        if (!ReadNumber(inner, pos, b)) {
            sx = sy = a; // scale(s): uniform scale
            return true;
        }
        sx = a;
        sy = b;
        return true;
    }

    void ApplyScale(LottieBezier &subpath, double sx, double sy)
    {
        auto scale = [sx, sy](LottieVec &v) {
            v.x *= sx;
            v.y *= sy;
        };
        for (LottieVec &v : subpath.v) scale(v);
        for (LottieVec &v : subpath.i) scale(v);
        for (LottieVec &v : subpath.o) scale(v);
    }

} // namespace

//----------------------------------------------------------------------------
// Public API
//----------------------------------------------------------------------------

bool ParseSvgPathData(const std::string &d, std::vector<LottieBezier> &paths)
{
    PathBuilder builder(paths);
    return builder.Parse(d);
}

bool ParseGlyphXml(const std::string &xml, std::vector<LottieBezier> &paths)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result result = doc.load_buffer(xml.c_str(), xml.size());
    if (!result) {
        LogWarning("ParseGlyphXml: could not parse glyph XML (%s)", result.description());
        return false;
    }

    std::vector<pugi::xml_node> pathNodes;
    CollectPathNodes(doc, pathNodes);

    bool success = true;
    for (const pugi::xml_node &pathNode : pathNodes) {
        double sx = 1.0, sy = 1.0;
        const pugi::xml_attribute transform = pathNode.attribute("transform");
        if (transform) {
            if (!ParseScaleTransform(transform.value(), sx, sy)) {
                LogWarning("ParseGlyphXml: unsupported transform '%s', assuming no scale", transform.value());
                sx = sy = 1.0;
            }
        }

        std::vector<LottieBezier> localPaths;
        if (!ParseSvgPathData(pathNode.attribute("d").value(), localPaths)) {
            success = false;
            continue;
        }

        for (LottieBezier &subpath : localPaths) {
            ApplyScale(subpath, sx, sy);
            paths.push_back(std::move(subpath));
        }
    }
    return success;
}

} // namespace vrv
