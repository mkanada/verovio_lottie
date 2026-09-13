/////////////////////////////////////////////////////////////////////////////
// Name:        lottiewriter.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_WRITER_H__
#define __VRV_LOTTIE_WRITER_H__

#include <string>
#include <vector>

//----------------------------------------------------------------------------

#include "lottiegeometry.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottieWriter
//----------------------------------------------------------------------------

/**
 * This class serializes the internal Lottie IR (LottiePage / LottieNode / LottieShape)
 * built by LottieDeviceContext into a Lottie animation JSON document, with one layer
 * per page.
 */
class LottieWriter {
public:
    /**
     * Serialize an animation with one layer per page.
     */
    static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name);
};

} // namespace vrv

#endif // __VRV_LOTTIE_WRITER_H__
