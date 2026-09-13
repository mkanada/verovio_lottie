/////////////////////////////////////////////////////////////////////////////
// Name:        csscolor.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "csscolor.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <vector>

//----------------------------------------------------------------------------

#include "devicecontextbase.h"
#include "vrv.h"

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// Local helpers
//----------------------------------------------------------------------------

namespace {

    bool ParseHexColor(const std::string &css, int &outColor)
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

    // rgb(r,g,b), integer components 0-255 (out-of-range values are clamped, as CSS requires).
    bool ParseRgbFunction(const std::string &lowerCss, int &outColor)
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
    bool ParseNamedColor(const std::string &lowerCss, int &outColor)
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

} // namespace

int ResolveColor(const std::string &colorCss, int inheritedColor)
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
        LogWarning("ResolveColor: unsupported CSS color value '%s'; defaulting to black.", trimmed.c_str());
    }
    return COLOR_BLACK;
}

} // namespace vrv
