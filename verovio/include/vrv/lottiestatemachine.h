/////////////////////////////////////////////////////////////////////////////
// Name:        lottiestatemachine.h
// Author:      Verovio Team
// Created:     2026
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_LOTTIE_STATE_MACHINE_H__
#define __VRV_LOTTIE_STATE_MACHINE_H__

#include <string>
#include <vector>

namespace vrv {

//----------------------------------------------------------------------------
// LottieSMTransition, LottieSMState, LottieStateMachine
//----------------------------------------------------------------------------

/**
 * A single guarded transition, always living on a GlobalState in the star topology
 * validated by the B01 spike: it fires from whatever the current state is (not just
 * from the state it is declared on) as soon as its named Event input is fired by the
 * host. Several transitions may share the same toState with different eventInput
 * values - this is the whole mechanism behind grouping simultaneous notes (M2, see
 * docs/plano/decisoes/B02-mecanismo-destaque.md): no dedicated field is needed for it.
 */
struct LottieSMTransition {
    std::string toState;
    std::string eventInput; // name of the Event input that fires this transition
};

/**
 * A state machine state. PlaybackState plays a named marker (segment) of an animation;
 * GlobalState carries no animation of its own but its transitions are evaluated
 * regardless of the current state, which is what gives the star topology (any state
 * reachable directly from any other, confirmed in B01 E1/E4).
 */
struct LottieSMState {
    std::string name;
    bool isGlobal = false; // false = PlaybackState, true = GlobalState
    // PlaybackState only:
    std::string animation;
    std::string segment; // marker name
    bool autoplay = true;
    bool loop = false;
    // GlobalState only in the star topology (PlaybackStates leave this empty):
    std::vector<LottieSMTransition> transitions;
};

/**
 * A full dotLottie v2 state machine, serialized by LottieWriter::WriteStateMachine
 * into one s/<id>.json entry of the package.
 */
struct LottieStateMachine {
    std::string id;
    std::string initial;
    std::vector<LottieSMState> states;
};

} // namespace vrv

#endif // __VRV_LOTTIE_STATE_MACHINE_H__
