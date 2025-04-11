/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_RENDERINGPHASE_H_
#define MOZILLA_RENDERINGPHASE_H_

#include <cstdint>
#include "mozilla/EnumSet.h"

namespace mozilla {

// Steps in https://html.spec.whatwg.org/#update-the-rendering
// When updating this, please update sRenderingPhaseNames in nsRefreshDriver.
enum class RenderingPhase : uint8_t {
  // TODO: Reveal docs.
  FlushAutoFocusCandidates = 0,
  ResizeSteps,
  ScrollSteps,
  EvaluateMediaQueriesAndReportChanges,
  UpdateAnimationsAndSendEvents,
  FullscreenSteps,
  // TODO: Context lost steps?
  AnimationFrameCallbacks,
  // TODO(emilio): UpdateContentRelevancy is no longer a rendering phase of its
  // own (it should happen during resize observer handling).
  UpdateContentRelevancy,
  ResizeObservers,
  ViewTransitionOperations,
  UpdateIntersectionObservations,
  // TODO: Record rendering time
  // TODO: Mark paint timing
  Paint,
  // TODO: Process top layer removals.
  Count,
};

using RenderingPhases = EnumSet<RenderingPhase, uint16_t>;
inline constexpr RenderingPhases AllRenderingPhases() {
  return {
      RenderingPhase::FlushAutoFocusCandidates,
      RenderingPhase::ResizeSteps,
      RenderingPhase::ScrollSteps,
      RenderingPhase::EvaluateMediaQueriesAndReportChanges,
      RenderingPhase::UpdateAnimationsAndSendEvents,
      RenderingPhase::FullscreenSteps,
      RenderingPhase::AnimationFrameCallbacks,
      RenderingPhase::UpdateContentRelevancy,
      RenderingPhase::ResizeObservers,
      RenderingPhase::ViewTransitionOperations,
      RenderingPhase::UpdateIntersectionObservations,
      RenderingPhase::Paint,
  };
}

}  // namespace mozilla

#endif
