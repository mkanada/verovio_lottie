/////////////////////////////////////////////////////////////////////////////
// Name:        csscolor.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_CSS_COLOR_H__
#define __VRV_CSS_COLOR_H__

#include <string>

namespace vrv {

//----------------------------------------------------------------------------
// CSS color parsing
//----------------------------------------------------------------------------

/**
 * Resolves a CSS color value into a packed 24-bit color (COLOR_BLACK, COLOR_WHITE, parsed
 * #RRGGBB, etc.). Supports "#RGB", "#RRGGBB", "rgb(r,g,b)" and a subset of CSS named colors.
 * An empty colorCss returns inheritedColor unchanged; an unparseable, non-empty value is
 * logged once and defaults to black, matching the SVG behavior of falling back to the initial
 * "black" fill/stroke rather than silently inheriting.
 *
 * Shared by LottieWriter (resolving @color/SetCustomGraphicColor at write time) and
 * LottieDeviceContext::DrawSvgShape (resolving an embedded <svg> path's own fill/stroke
 * attributes at draw time).
 */
int ResolveColor(const std::string &colorCss, int inheritedColor);

} // namespace vrv

#endif // __VRV_CSS_COLOR_H__
