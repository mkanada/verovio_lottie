/////////////////////////////////////////////////////////////////////////////
// Name:        lottiepageturn.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_PAGE_TURN_H__
#define __VRV_LOTTIE_PAGE_TURN_H__

#include <string>
#include <vector>

//----------------------------------------------------------------------------

#include "lottiestatemachine.h"

namespace vrv {

struct LottieNode;
struct LottiePage;

//----------------------------------------------------------------------------
// LottiePageBoundary, LottiePageTurnLayout
//----------------------------------------------------------------------------

/**
 * The two discrete, directly-addressable events at one page boundary (docs/plano/decisoes/
 * B02-mecanismo-destaque.md, decision item 4): "peek" fires when playback enters the last
 * measure of the page before the boundary and partially reveals the next page; "cover" fires
 * when playback enters the first measure of the page after the boundary and completes the
 * transition. Addressed by the boundary measures' xml:id, not by note ids.
 */
struct LottiePageBoundary {
    int fromPage = 0; // 0-based index of the page before the boundary (the page after is fromPage + 1) -
        // BuildLayout only pushes an entry here for boundaries that actually got both event ids
        // (see "Decisões de escopo" in C04), so this can't be inferred from the vector index alone.
    std::string peekMarker; // e.g. "peek1"
    std::string coverMarker; // e.g. "cover1"
    std::string peekEventId; // xml:id of the last measure of the page before the boundary
    std::string coverEventId; // xml:id of the first measure of the page after the boundary
    int peekStartFrame = 0;
    int peekDurationFrames = 0;
    int coverStartFrame = 0;
    int coverDurationFrames = 0;
};

/**
 * The camera's frame layout across the whole horizontal page track: one resting marker per
 * page plus one peek+cover marker pair per boundary between consecutive pages, laid out as
 * sequential frame slots (LottiePageTurnBuilder::BuildLayout). LottieWriter::WriteAnimation
 * turns this into the actual camera layer/keyframes/markers; this struct only carries the
 * frame-numbers side of the policy (the pixel/color side stays in the writer, same split as
 * LottieHighlightGroup/LottieWriter for M2).
 */
struct LottiePageTurnLayout {
    bool enabled = false; // false = no page-turn content (one page, or nothing could be built)
    std::vector<std::string> pageMarkers; // "page0".."page{N-1}", parallel to the pages vector
    std::vector<int> pageRestFrames; // camera resting frame per page, parallel to pageMarkers
    std::vector<LottiePageBoundary> boundaries; // pages.size() - 1 entries, in page order
    int endFrame = 0; // last frame used by page-turn content (feeds the composition's "op")
};

//----------------------------------------------------------------------------
// LottiePageTurnBuilder
//----------------------------------------------------------------------------

/**
 * Builds the page-turn camera layout (frame numbers, boundary event ids) and the star-topology
 * state machine that addresses it, from the already-rendered LottiePage trees. Unlike
 * LottieHighlightBuilder, this does not need the Doc/timemap: boundary detection only needs the
 * ids already present in the rendered tree (every <measure> becomes a LottieNode with
 * className == "measure" and id == its xml:id, in document order - see View::DrawMeasure).
 */
class LottiePageTurnBuilder {
public:
    /**
     * Collects the xml:id of every <measure> LottieNode in the subtree rooted at `root`, in
     * document order (pre-order, not reversed - unlike the writer's paint-order traversal).
     * Only the first/last elements are used by BuildLayout; exposed separately so it stays
     * independently testable.
     */
    static std::vector<std::string> CollectMeasureIdsInOrder(const LottieNode &root);

    /**
     * Lays out one resting marker per page and one peek+cover marker pair per boundary between
     * consecutive pages, as sequential non-overlapping frame slots starting at `firstFrame`
     * (same NOTE_SLOT-style gapFrames fix as LottieHighlightBuilder::BuildGroups - see B01's
     * E2). Returns a disabled ({} ) layout when pages.size() <= 1. A page with no <measure> in
     * it cannot supply a boundary event id on its side: that particular boundary is skipped
     * (logged), but the page still gets its own resting marker/frame so the track stays
     * complete - see C04's "Decisões de escopo".
     */
    static LottiePageTurnLayout BuildLayout(const std::vector<const LottiePage *> &pages, int firstFrame,
        int peekDurationFrames, int coverDurationFrames, int gapFrames);

    /**
     * Builds the star-topology state machine addressing `layout`: one non-autoplayed
     * PlaybackState per page (the camera only moves when a boundary event fires it) plus one
     * autoplayed PlaybackState per peek/cover marker, and a single GlobalState with one
     * Transition per boundary event (peekEventId -> peek state, coverEventId -> cover state).
     */
    static LottieStateMachine BuildStateMachine(
        const LottiePageTurnLayout &layout, const std::string &animationId, const std::string &smId);
};

} // namespace vrv

#endif // __VRV_LOTTIE_PAGE_TURN_H__
