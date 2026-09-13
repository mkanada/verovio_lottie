/////////////////////////////////////////////////////////////////////////////
// Name:        lottiepageturn.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "lottiepageturn.h"

//----------------------------------------------------------------------------

#include "lottiegeometry.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottiePageTurnBuilder
//----------------------------------------------------------------------------

static void CollectMeasureIdsRecursive(const LottieNode &node, std::vector<std::string> &ids)
{
    if ((node.className == "measure") && !node.id.empty()) {
        ids.push_back(node.id);
    }
    for (const LottieChild &child : node.children) {
        if (child.group) {
            CollectMeasureIdsRecursive(*child.group, ids);
        }
    }
}

std::vector<std::string> LottiePageTurnBuilder::CollectMeasureIdsInOrder(const LottieNode &root)
{
    std::vector<std::string> ids;
    CollectMeasureIdsRecursive(root, ids);
    return ids;
}

LottiePageTurnLayout LottiePageTurnBuilder::BuildLayout(
    const std::vector<const LottiePage *> &pages, int firstFrame, int peekDurationFrames, int coverDurationFrames,
    int gapFrames)
{
    LottiePageTurnLayout layout;
    if (pages.size() <= 1) {
        return layout;
    }

    layout.enabled = true;
    const std::size_t pageCount = pages.size();
    layout.pageMarkers.resize(pageCount);
    layout.pageRestFrames.resize(pageCount);

    std::vector<std::string> firstMeasureId(pageCount);
    std::vector<std::string> lastMeasureId(pageCount);
    for (std::size_t i = 0; i < pageCount; ++i) {
        const std::vector<std::string> measureIds = CollectMeasureIdsInOrder(*pages[i]->root);
        if (measureIds.empty()) {
            LogWarning(
                "LottiePageTurnBuilder::BuildLayout: page %d has no <measure> - the page-turn boundaries "
                "touching it will be skipped (it still gets its own camera resting position)",
                static_cast<int>(i) + 1);
            continue;
        }
        firstMeasureId[i] = measureIds.front();
        lastMeasureId[i] = measureIds.back();
    }

    int frame = firstFrame;
    layout.pageMarkers[0] = "page0";
    layout.pageRestFrames[0] = frame;

    for (std::size_t i = 0; i + 1 < pageCount; ++i) {
        // The frame slot width is reserved unconditionally (whether or not this boundary ends
        // up with an event to fire it) so that pageRestFrames stays strictly increasing - two
        // pages can never end up sharing one camera-keyframe frame number with different values.
        const int peekStartFrame = frame;
        frame += peekDurationFrames + gapFrames;
        const int coverStartFrame = frame;
        frame += coverDurationFrames + gapFrames;

        if (!lastMeasureId[i].empty() && !firstMeasureId[i + 1].empty()) {
            LottiePageBoundary boundary;
            boundary.fromPage = static_cast<int>(i);
            boundary.peekMarker = "peek" + std::to_string(i + 1);
            boundary.coverMarker = "cover" + std::to_string(i + 1);
            boundary.peekEventId = lastMeasureId[i];
            boundary.coverEventId = firstMeasureId[i + 1];
            boundary.peekStartFrame = peekStartFrame;
            boundary.peekDurationFrames = peekDurationFrames;
            boundary.coverStartFrame = coverStartFrame;
            boundary.coverDurationFrames = coverDurationFrames;
            layout.boundaries.push_back(boundary);
        }

        layout.pageMarkers[i + 1] = "page" + std::to_string(i + 1);
        layout.pageRestFrames[i + 1] = frame;
    }

    // "op" (the composition's out-point) is an exclusive upper bound - confirmed empirically
    // against dotlottie-rs while validating C04 (set_frame(op) fails with "invalid parameter";
    // op - 1 is the last valid frame). Without this extra frame, the last page's own resting
    // marker would sit exactly at "op" and be unreachable.
    layout.endFrame = frame + 1;
    return layout;
}

LottieStateMachine LottiePageTurnBuilder::BuildStateMachine(
    const LottiePageTurnLayout &layout, const std::string &animationId, const std::string &smId)
{
    LottieStateMachine sm;
    sm.id = smId;
    sm.initial = "page0";

    for (const std::string &marker : layout.pageMarkers) {
        LottieSMState state;
        state.name = marker;
        state.animation = animationId;
        state.segment = marker;
        state.autoplay = false;
        state.loop = false;
        sm.states.push_back(state);
    }

    LottieSMState global;
    global.name = "GLOBAL";
    global.isGlobal = true;

    for (const LottiePageBoundary &boundary : layout.boundaries) {
        LottieSMState peek;
        peek.name = boundary.peekMarker;
        peek.animation = animationId;
        peek.segment = boundary.peekMarker;
        peek.autoplay = true;
        peek.loop = false;
        sm.states.push_back(peek);

        LottieSMState cover;
        cover.name = boundary.coverMarker;
        cover.animation = animationId;
        cover.segment = boundary.coverMarker;
        cover.autoplay = true;
        cover.loop = false;
        sm.states.push_back(cover);

        global.transitions.push_back({ boundary.peekMarker, boundary.peekEventId });
        global.transitions.push_back({ boundary.coverMarker, boundary.coverEventId });
    }

    sm.states.push_back(global);
    return sm;
}

} // namespace vrv
