/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSourceByActorId } from "./sources.js";
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");

export function getSelectedTraceIndex(state) {
  return state.tracerFrames?.selectedTraceIndex;
}
export function getFilteredTopTraces(state) {
  return state.tracerFrames?.mutableFilteredTopTraces || [];
}
export function getAllTraces(state) {
  return state.tracerFrames?.mutableTraces || [];
}
export function getTraceChildren(state) {
  return state.tracerFrames?.mutableChildren || [];
}
export function getTraceParents(state) {
  return state.tracerFrames?.mutableParents || [];
}
export function getTraceFrames(state) {
  return state.tracerFrames?.mutableFrames || [];
}
export function getAllMutationTraces(state) {
  return state.tracerFrames?.mutableMutationTraces || [];
}
export function getAllTraceCount(state) {
  return state.tracerFrames?.mutableTraces.length || 0;
}
export function getRuntimeVersions(state) {
  return {
    localPlatformVersion: state.tracerFrames?.localPlatformVersion,
    remotePlatformVersion: state.tracerFrames?.remotePlatformVersion,
  };
}
export function getTracerEventNames(state) {
  return state.tracerFrames?.mutableEventNames;
}
export function getTraceDomEvent(state) {
  return state.tracerFrames?.domEvents || new Set();
}
export function getTraceHighlightedDomEvents(state) {
  return state.tracerFrames?.highlightedDomEvents || [];
}
export function getSelectedTraceSource(state) {
  const trace = getAllTraces(state)[getSelectedTraceIndex(state)];
  if (!trace) {
    return null;
  }
  const frameIndex = trace[TRACER_FIELDS_INDEXES.FRAME_INDEX];
  const frames = getTraceFrames(state);
  const frame = frames[frameIndex];
  if (!frame) {
    return null;
  }
  return getSourceByActorId(state, frame.sourceId);
}
export function getTraceMatchingSearchTraces(state) {
  return state.tracerFrames?.mutableMatchingTraces || [];
}
export function getTraceMatchingSearchException(state) {
  return state.tracerFrames?.searchExceptionMessage || null;
}
export function getTraceMatchingSearchValueOrGrip(state) {
  return state.tracerFrames?.searchValueOrGrip;
}
export function getIsTracingValues(state) {
  return state.tracerFrames?.traceValues || false;
}
