/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
