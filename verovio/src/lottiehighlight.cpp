/////////////////////////////////////////////////////////////////////////////
// Name:        lottiehighlight.cpp
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#include "lottiehighlight.h"

//----------------------------------------------------------------------------

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>

//----------------------------------------------------------------------------

#include "doc.h"
#include "lottiegeometry.h"
#include "midifunctor.h"
#include "timemap.h"
#include "vrv.h"

namespace vrv {

//----------------------------------------------------------------------------
// LottieHighlightBuilder
//----------------------------------------------------------------------------

static void CollectIdsRecursive(const LottieNode &node, std::unordered_map<std::string, int> &counts)
{
    if (!node.id.empty()) {
        ++counts[node.id];
    }
    for (const LottieChild &child : node.children) {
        if (child.group) {
            CollectIdsRecursive(*child.group, counts);
        }
    }
}

std::unordered_set<std::string> LottieHighlightBuilder::CollectIds(const LottieNode &root)
{
    std::unordered_map<std::string, int> counts;
    CollectIdsRecursive(root, counts);

    // Validação de unicidade (docs/plano/C03-slots-interativos.md): CollectIds itself only
    // needs a set (membership), but a duplicate xml:id across two different elements would
    // silently merge their highlight/interactive addressing under a single group/slot - worth
    // a warning even though it doesn't stop the export (this is a source-document quality
    // issue, not something this exporter can fix).
    std::unordered_set<std::string> ids;
    ids.reserve(counts.size());
    for (const auto &[id, count] : counts) {
        if (count > 1) {
            LogWarning(
                "LottieHighlightBuilder::CollectIds: xml:id '%s' appears %d times in the rendered page - "
                "highlight/interactive addressing for it will be ambiguous (duplicate xml:id in the source?)",
                id.c_str(), count);
        }
        ids.insert(id);
    }
    return ids;
}

// Validação de nomes (docs/plano/C03-slots-interativos.md): "idle"/"GLOBAL" and the "hl<N>"
// pattern are reserved by BuildStateMachine/BuildGroups below for control states. A real
// xml:id colliding with one of these would make the star topology ambiguous. There is no
// character-set restriction to enforce here (see the C03 plan for why - dotlottie-rs compares
// these names as plain strings), only this reserved-name check.
static bool IsReservedHighlightName(const std::string &id)
{
    if (id == "idle" || id == "GLOBAL") return true;
    if (id.size() > 2 && id[0] == 'h' && id[1] == 'l') {
        return std::all_of(id.begin() + 2, id.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    }
    return false;
}

std::vector<LottieHighlightGroup> LottieHighlightBuilder::BuildGroups(
    Doc &doc, const std::unordered_set<std::string> &ids, int firstFrame, int durationFrames, int gapFrames)
{
    for (const std::string &id : ids) {
        if (IsReservedHighlightName(id)) {
            LogWarning(
                "LottieHighlightBuilder::BuildGroups: xml:id '%s' collides with a name reserved for state-machine "
                "control ('idle', 'GLOBAL', or 'hl<N>') - its highlight/interactive addressing may be ambiguous",
                id.c_str());
        }
    }

    if (!doc.HasTimemap()) {
        doc.CalculateTimemap();
    }

    Timemap timemap;
    GenerateTimemapFunctor generateTimemap(&timemap);
    generateTimemap.SetNoCue(doc.GetOptions()->m_midiNoCue.GetValue());
    doc.Process(generateTimemap);

    std::vector<LottieHighlightGroup> groups;
    for (const auto &[qstamp, entry] : timemap.GetMap()) {
        std::vector<std::string> members;
        for (const std::string &id : entry.notesOn) {
            if (ids.find(id) != ids.end()) {
                members.push_back(id);
            }
        }
        if (members.empty()) continue;

        LottieHighlightGroup group;
        group.name = "hl" + std::to_string(groups.size());
        group.startFrame = firstFrame + static_cast<int>(groups.size()) * (durationFrames + gapFrames);
        group.durationFrames = durationFrames;
        group.memberIds = members;
        groups.push_back(group);
    }

    return groups;
}

LottieStateMachine LottieHighlightBuilder::BuildStateMachine(
    const std::vector<LottieHighlightGroup> &groups, const std::string &animationId, const std::string &smId)
{
    LottieStateMachine sm;
    sm.id = smId;
    sm.initial = "idle";

    LottieSMState idle;
    idle.name = "idle";
    idle.animation = animationId;
    idle.segment = "idle";
    idle.autoplay = false;
    idle.loop = false;
    sm.states.push_back(idle);

    for (const LottieHighlightGroup &group : groups) {
        LottieSMState state;
        state.name = group.name;
        state.animation = animationId;
        state.segment = group.name;
        state.autoplay = true;
        state.loop = false;
        sm.states.push_back(state);
    }

    LottieSMState global;
    global.name = "GLOBAL";
    global.isGlobal = true;
    for (const LottieHighlightGroup &group : groups) {
        for (const std::string &memberId : group.memberIds) {
            global.transitions.push_back({ group.name, memberId });
        }
    }
    // M2->M3 handoff (docs/plano/C03-slots-interativos.md): lets the host force this
    // automatic engine back to rest before switching to interactive-mode color slots
    // (fire("idle")), instead of leaving a highlight "stuck" mid-fade with no way back.
    global.transitions.push_back({ "idle", "idle" });
    sm.states.push_back(global);

    return sm;
}

} // namespace vrv
