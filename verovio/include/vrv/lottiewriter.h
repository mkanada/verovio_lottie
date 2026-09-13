/////////////////////////////////////////////////////////////////////////////
// Name:        lottiewriter.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_WRITER_H__
#define __VRV_LOTTIE_WRITER_H__

#include <string>
#include <unordered_set>
#include <vector>

//----------------------------------------------------------------------------

#include "lottiegeometry.h"
#include "lottiehighlight.h"
#include "lottiepageturn.h"
#include "lottiestatemachine.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottieWriter
//----------------------------------------------------------------------------

/**
 * This class serializes the internal Lottie IR (LottiePage / LottieNode / LottieShape,
 * LottieStateMachine) built by LottieDeviceContext and by the state machine builders into
 * the JSON documents of a dotLottie package.
 */
class LottieWriter {
public:
    /**
     * Serialize an animation with one layer per page. highlightGroups (M2, see
     * docs/plano/C02-notas-animadas.md) is optional: passing none (the default) reproduces
     * the exact byte output of before C02 - empty markers array, plain static fill/stroke
     * colors. When non-empty, every shape whose enclosing LottieNode::id matches a group
     * member gets a keyframed fill/stroke color (its own resolved color, flashing to
     * highlightColor and fading back over the group's frame slot) instead of a static one,
     * the composition's "markers" array gets one entry per group (plus a 1-frame "idle"
     * marker at frame 0), and the last page's out-point is stretched to cover the furthest
     * group's end frame.
     *
     * interactiveIds (M3, see docs/plano/C03-slots-interativos.md) is also optional and
     * independent of highlightGroups: every shape whose enclosing LottieNode::id is in this
     * set gets a "sid" on its fill/stroke color property (equal to that id), so the host can
     * address it directly with Player::set_color_slot regardless of whether it is also part
     * of a highlightGroups entry. Passing none reproduces the exact byte output of before C03.
     *
     * pageTurn (see docs/plano/C04-paginas-virada.md) is also optional and independent of the
     * two parameters above: when pageTurn.enabled, every page layer is repositioned onto a
     * horizontal track (x = pageIndex * trackStep, trackStep being the composition's own width)
     * and parented to a new null "camera" layer whose x position is keyframed across
     * pageTurn's markers (peekFraction controls how far the camera "peeks" toward the next page
     * before "covering" the rest of the move - see C04; 0.08 was picked empirically while
     * validating this step - anything above ~0.15 starts sliding most of the current page out
     * of view during "peek" instead of just hinting at the next one at the edge). Passing a
     * default-constructed LottiePageTurnLayout (enabled == false, the default) reproduces the
     * exact byte output of before C04 - no camera layer, no page markers, each page layer keeps
     * today's own ip/op windowing.
     */
    static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name,
        const std::vector<LottieHighlightGroup> &highlightGroups = {}, int highlightColor = 0xE53935,
        const std::unordered_set<std::string> &interactiveIds = {}, const LottiePageTurnLayout &pageTurn = {},
        double peekFraction = 0.08);

    /**
     * Serialize a state machine (dotLottie v2 format, validated against dotlottie-rs by the
     * B01 spike), for one s/<id>.json entry of the package.
     */
    static std::string WriteStateMachine(const LottieStateMachine &stateMachine);

    /**
     * Serialize a dotLottie manifest.json. stateMachineIds/initialStateMachine are optional:
     * omit them to get a manifest without a "stateMachines" entry (as produced before this
     * was extracted from Toolkit::RenderToDotLottieFile).
     */
    static std::string WriteManifest(const std::string &generator, const std::vector<std::string> &animationIds,
        const std::string &initialAnimation, const std::vector<std::string> &stateMachineIds = {},
        const std::string &initialStateMachine = "");
};

} // namespace vrv

#endif // __VRV_LOTTIE_WRITER_H__
