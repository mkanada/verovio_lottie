/////////////////////////////////////////////////////////////////////////////
// Name:        lottiehighlight.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_HIGHLIGHT_H__
#define __VRV_LOTTIE_HIGHLIGHT_H__

#include <string>
#include <unordered_set>
#include <vector>

//----------------------------------------------------------------------------

#include "lottiestatemachine.h"

namespace vrv {

class Doc;
struct LottieNode;

//----------------------------------------------------------------------------
// LottieHighlightGroup
//----------------------------------------------------------------------------

/**
 * A group of notes that share the same timemap onset instant (M2 grouping, see
 * docs/plano/decisoes/B02-mecanismo-destaque.md): any of their xml:ids fires the same
 * highlight marker/state. Reserves a dedicated, non-overlapping frame slot within the
 * shared "score" animation timeline.
 */
struct LottieHighlightGroup {
    std::string name; // internal marker/state name (opaque - the real address is memberIds)
    int startFrame = 0;
    int durationFrames = 0;
    std::vector<std::string> memberIds; // xml:ids that fire this group
};

//----------------------------------------------------------------------------
// LottieHighlightBuilder
//----------------------------------------------------------------------------

/**
 * Builds the note-highlight groups (M2) and the star-topology state machine that addresses
 * them, from a Doc's timemap and a rendered LottiePage's ids. Does not touch geometry/color -
 * that part is done by LottieWriter::WriteAnimation from the groups this class produces.
 */
class LottieHighlightBuilder {
public:
    /**
     * Collects every non-empty LottieNode::id in the subtree rooted at `root` (document
     * order not preserved - a plain set, since callers only need membership).
     */
    static std::unordered_set<std::string> CollectIds(const LottieNode &root);

    /**
     * Groups the ids in `ids` by exact timemap onset instant (M2), in onset order, laying
     * them out as sequential, non-overlapping frame slots starting at `firstFrame`. Each
     * slot is `durationFrames + gapFrames` wide but only the first `durationFrames` are
     * used by the group's own marker - the extra gapFrames avoid the boundary-frame bug
     * documented in compare/fixtures/b01/gen.py (NOTE_SLOT) and confirmed in B01's E2.
     * Runs the timemap functor on `doc` directly (not a cloned MIDI doc), so the ids match
     * exactly what was rendered - see C02's "Fora de escopo" for the repeat-expansion caveat.
     */
    static std::vector<LottieHighlightGroup> BuildGroups(
        Doc &doc, const std::unordered_set<std::string> &ids, int firstFrame, int durationFrames, int gapFrames);

    /**
     * Builds the star-topology state machine addressing `groups`: one PlaybackState per
     * group (segment == group.name) plus a single GlobalState with one transition per
     * member id of every group (several transitions may share the same toState - the
     * mechanism validated by C01's Risk-1 test), and an idle PlaybackState (segment
     * "idle", not autoplayed) as the initial state.
     */
    static LottieStateMachine BuildStateMachine(
        const std::vector<LottieHighlightGroup> &groups, const std::string &animationId, const std::string &smId);
};

} // namespace vrv

#endif // __VRV_LOTTIE_HIGHLIGHT_H__
