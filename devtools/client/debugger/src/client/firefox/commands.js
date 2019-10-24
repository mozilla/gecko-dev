/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { prepareSourcePayload, createThread } from "./create";
import { updateTargets } from "./targets";
import { clientEvents } from "./events";

import Reps from "devtools-reps";
import type { Node } from "devtools-reps";

import type {
  ActorId,
  BreakpointLocation,
  BreakpointOptions,
  PendingLocation,
  Frame,
  FrameId,
  GeneratedSourceData,
  Script,
  SourceId,
  SourceActor,
  Range,
  Thread,
  ThreadType,
} from "../../types";

import type {
  Target,
  DebuggerClient,
  Grip,
  ThreadFront,
  ObjectFront,
  SourcesPacket,
} from "./types";

import type {
  EventListenerCategoryList,
  EventListenerActiveList,
} from "../../actions/types";

let targets: { [ThreadType]: { [string]: Target } };
let currentThreadFront: ThreadFront;
let currentTarget: Target;
let debuggerClient: DebuggerClient;
let sourceActors: { [ActorId]: SourceId };
let breakpoints: { [string]: Object };
let eventBreakpoints: ?EventListenerActiveList;

type Dependencies = {
  threadFront: ThreadFront,
  tabTarget: Target,
  debuggerClient: DebuggerClient,
};

function setupCommands(dependencies: Dependencies) {
  currentThreadFront = dependencies.threadFront;
  currentTarget = dependencies.tabTarget;
  debuggerClient = dependencies.debuggerClient;
  targets = { worker: {}, contentProcess: {} };
  sourceActors = {};
  breakpoints = {};
}

function createObjectFront(grip: Grip) {
  return debuggerClient.createObjectFront(grip);
}

async function loadObjectProperties(root: Node) {
  const utils = Reps.objectInspector.utils;
  const properties = await utils.loadProperties.loadItemProperties(
    root,
    debuggerClient
  );
  return utils.node.getChildren({
    item: root,
    loadedProperties: new Map([[root.path, properties]]),
  });
}

function releaseActor(actor: String) {
  if (!actor) {
    return;
  }

  const objFront = debuggerClient.getFrontByID(actor);

  if (objFront) {
    return objFront.release().catch(() => {});
  }
}

function sendPacket(packet: Object) {
  return debuggerClient.request(packet);
}

// Transforms targets from {[ThreadType]: TargetMap} to TargetMap
function getTargetsMap(): { string: Target } {
  return Object.assign({}, ...Object.values(targets));
}

function lookupTarget(thread: string) {
  if (thread == currentThreadFront.actor) {
    return currentTarget;
  }

  const targetsMap = getTargetsMap();
  if (!targetsMap[thread]) {
    throw new Error(`Unknown thread front: ${thread}`);
  }

  return targetsMap[thread];
}

function lookupThreadFront(thread: string) {
  const target = lookupTarget(thread);
  return target.threadFront;
}

function listThreadFronts() {
  const targetList = (Object.values(getTargetsMap()): any);
  return targetList.map(target => target.threadFront);
}

function forEachThread(iteratee) {
  // We have to be careful here to atomically initiate the operation on every
  // thread, with no intervening await. Otherwise, other code could run and
  // trigger additional thread operations. Requests on server threads will
  // resolve in FIFO order, and this could result in client and server state
  // going out of sync.

  const promises = [currentThreadFront, ...listThreadFronts()].map(
    // If a thread shuts down while sending the message then it will
    // throw. Ignore these exceptions.
    t => iteratee(t).catch(e => console.log(e))
  );

  return Promise.all(promises);
}

function resume(thread: string): Promise<*> {
  return lookupThreadFront(thread).resume();
}

function stepIn(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepIn();
}

function stepOver(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepOver();
}

function stepOut(thread: string): Promise<*> {
  return lookupThreadFront(thread).stepOut();
}

function rewind(thread: string): Promise<*> {
  return lookupThreadFront(thread).rewind();
}

function reverseStepOver(thread: string): Promise<*> {
  return lookupThreadFront(thread).reverseStepOver();
}

function breakOnNext(thread: string): Promise<*> {
  return lookupThreadFront(thread).breakOnNext();
}

async function sourceContents({
  actor,
  thread,
}: SourceActor): Promise<{| source: any, contentType: ?string |}> {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  const { source, contentType } = await sourceFront.source();
  return { source, contentType };
}

function setXHRBreakpoint(path: string, method: string) {
  return currentThreadFront.setXHRBreakpoint(path, method);
}

function removeXHRBreakpoint(path: string, method: string) {
  return currentThreadFront.removeXHRBreakpoint(path, method);
}

function addWatchpoint(
  object: Grip,
  property: string,
  label: string,
  watchpointType: string
) {
  if (currentTarget.traits.watchpoints) {
    const objectFront = createObjectFront(object);
    return objectFront.addWatchpoint(property, label, watchpointType);
  }
}

function removeWatchpoint(object: Grip, property: string) {
  if (currentTarget.traits.watchpoints) {
    const objectFront = createObjectFront(object);
    return objectFront.removeWatchpoint(property);
  }
}

// Get the string key to use for a breakpoint location.
// See also duplicate code in breakpoint-actor-map.js :(
function locationKey(location: BreakpointLocation) {
  const { sourceUrl, line, column } = location;
  const sourceId = location.sourceId || "";
  // $FlowIgnore
  return `${sourceUrl}:${sourceId}:${line}:${column}`;
}

function detachWorkers() {
  for (const thread of listThreadFronts()) {
    thread.detach();
  }
}

function maybeGenerateLogGroupId(options) {
  if (
    options.logValue &&
    currentTarget.traits &&
    currentTarget.traits.canRewind
  ) {
    return { ...options, logGroupId: `logGroup-${Math.random()}` };
  }
  return options;
}

function maybeClearLogpoint(location: BreakpointLocation) {
  const bp = breakpoints[locationKey(location)];
  if (bp && bp.options.logGroupId && currentTarget.activeConsole) {
    currentTarget.activeConsole.emit(
      "clearLogpointMessages",
      bp.options.logGroupId
    );
  }
}

function hasBreakpoint(location: BreakpointLocation) {
  return !!breakpoints[locationKey(location)];
}

function setBreakpoint(
  location: BreakpointLocation,
  options: BreakpointOptions
) {
  maybeClearLogpoint(location);
  options = maybeGenerateLogGroupId(options);
  breakpoints[locationKey(location)] = { location, options };

  return forEachThread(thread => thread.setBreakpoint(location, options));
}

function removeBreakpoint(location: PendingLocation) {
  maybeClearLogpoint((location: any));
  delete breakpoints[locationKey((location: any))];

  return forEachThread(thread => thread.removeBreakpoint(location));
}

async function evaluateInFrame(script: Script, options: EvaluateParam) {
  return evaluate(script, options);
}

async function evaluateExpressions(scripts: Script[], options: EvaluateParam) {
  return Promise.all(scripts.map(script => evaluate(script, options)));
}

type EvaluateParam = { thread: string, frameId: ?FrameId };

function evaluate(
  script: ?Script,
  { thread, frameId }: EvaluateParam = {}
): Promise<{ result: Grip | null }> {
  const params = { thread, frameActor: frameId };
  if (!currentTarget || !script) {
    return Promise.resolve({ result: null });
  }

  const target = thread ? lookupTarget(thread) : currentTarget;
  const console = target.activeConsole;
  if (!console) {
    return Promise.resolve({ result: null });
  }

  return console.evaluateJSAsync(script, params);
}

function autocomplete(
  input: string,
  cursor: number,
  frameId: ?string
): Promise<mixed> {
  if (!currentTarget || !currentTarget.activeConsole || !input) {
    return Promise.resolve({});
  }
  return new Promise(resolve => {
    currentTarget.activeConsole.autocomplete(
      input,
      cursor,
      result => resolve(result),
      frameId
    );
  });
}

function navigate(url: string): Promise<*> {
  return currentTarget.navigateTo({ url });
}

function reload(): Promise<*> {
  return currentTarget.reload();
}

function getProperties(thread: string, grip: Grip): Promise<*> {
  const objClient = lookupThreadFront(thread).pauseGrip(grip);

  return objClient.getPrototypeAndProperties().then(resp => {
    const { ownProperties, safeGetterValues } = resp;
    for (const name in safeGetterValues) {
      const { enumerable, writable, getterValue } = safeGetterValues[name];
      ownProperties[name] = { enumerable, writable, value: getterValue };
    }
    return resp;
  });
}

async function getFrameScopes(frame: Frame): Promise<*> {
  if (frame.scope) {
    return frame.scope;
  }

  const sourceThreadFront = lookupThreadFront(frame.thread);
  return sourceThreadFront.getEnvironment(frame.id);
}

function pauseOnExceptions(
  shouldPauseOnExceptions: boolean,
  shouldPauseOnCaughtExceptions: boolean
): Promise<*> {
  return forEachThread(thread =>
    thread.pauseOnExceptions(
      shouldPauseOnExceptions,
      // Providing opposite value because server
      // uses "shouldIgnoreCaughtExceptions"
      !shouldPauseOnCaughtExceptions
    )
  );
}

async function blackBox(
  sourceActor: SourceActor,
  isBlackBoxed: boolean,
  range?: Range
): Promise<*> {
  const sourceFront = currentThreadFront.source({ actor: sourceActor.actor });
  if (isBlackBoxed) {
    await sourceFront.unblackBox(range);
  } else {
    await sourceFront.blackBox(range);
  }
}

function setSkipPausing(shouldSkip: boolean) {
  return forEachThread(thread => thread.skipBreakpoints(shouldSkip));
}

function interrupt(thread: string): Promise<*> {
  return lookupThreadFront(thread).interrupt();
}

function setEventListenerBreakpoints(ids: string[]) {
  eventBreakpoints = ids;

  return forEachThread(thread => thread.setActiveEventBreakpoints(ids));
}

// eslint-disable-next-line
async function getEventListenerBreakpointTypes(): Promise<EventListenerCategoryList> {
  let categories;
  try {
    categories = await currentThreadFront.getAvailableEventBreakpoints();

    if (!Array.isArray(categories)) {
      // When connecting to older browser that had our placeholder
      // implementation of the 'getAvailableEventBreakpoints' endpoint, we
      // actually get back an object with a 'value' property containing
      // the categories. Since that endpoint wasn't actually backed with a
      // functional implementation, we just bail here instead of storing the
      // 'value' property into the categories.
      categories = null;
    }
  } catch (err) {
    // Event bps aren't supported on this firefox version.
  }
  return categories || [];
}

function pauseGrip(thread: string, func: Function): ObjectFront {
  return lookupThreadFront(thread).pauseGrip(func);
}

function registerSourceActor(sourceActorId: string, sourceId: SourceId) {
  sourceActors[sourceActorId] = sourceId;
}

async function getSources(
  client: ThreadFront
): Promise<Array<GeneratedSourceData>> {
  const { sources }: SourcesPacket = await client.getSources();

  return sources.map(source => prepareSourcePayload(client, source));
}

async function toggleEventLogging(logEventBreakpoints: boolean) {
  return forEachThread(thread =>
    thread.toggleEventLogging(logEventBreakpoints)
  );
}

function getAllThreadFronts() {
  const fronts = [currentThreadFront];
  for (const targetsForType of (Object.values(targets): any)) {
    for (const { threadFront } of (Object.values(targetsForType): any)) {
      fronts.push(threadFront);
    }
  }
  return fronts;
}

// Fetch the sources for all the targets
async function fetchSources(): Promise<Array<GeneratedSourceData>> {
  let sources = [];
  for (const threadFront of getAllThreadFronts()) {
    sources = sources.concat(await getSources(threadFront));
  }
  return sources;
}

// Check if any of the targets were paused before we opened
// the debugger. If one is paused. Fake a `pause` RDP event
// by directly calling the client event listener.
async function checkIfAlreadyPaused() {
  for (const threadFront of getAllThreadFronts()) {
    const pausedPacket = threadFront.getLastPausePacket();
    if (pausedPacket) {
      clientEvents.paused(threadFront, pausedPacket);
    }
  }
}

function getSourceForActor(actor: ActorId) {
  if (!sourceActors[actor]) {
    throw new Error(`Unknown source actor: ${actor}`);
  }
  return sourceActors[actor];
}

async function fetchThreads(type: ?ThreadType): Promise<Thread[]> {
  if (!type) {
    const workers = await updateThreads("worker");
    const processes = await updateThreads("contentProcess");
    return [...workers, ...processes];
  }

  return updateThreads(type);
}

async function updateThreads(type: ThreadType) {
  const options = {
    breakpoints,
    eventBreakpoints,
    observeAsmJS: true,
  };

  const newTargets = await updateTargets(type, {
    currentTarget,
    debuggerClient,
    targets,
    options,
  });

  // Fetch the sources and install breakpoints on any new workers.
  // NOTE: This runs in the background and fails quitely because it is
  // pretty easy for sources to throw during the fetch if their thread
  // shuts down, which would cause test failures.
  for (const actor in newTargets) {
    if (!targets[type][actor]) {
      const { threadFront } = newTargets[actor];
      getSources(threadFront).catch(e => console.error(e));
    }
  }

  targets = { ...targets, [type]: newTargets };

  return Object.keys(newTargets).map(actor =>
    createThread(actor, newTargets[actor])
  );
}

function getMainThread() {
  return currentThreadFront.actor;
}

async function getSourceActorBreakpointPositions(
  { thread, actor }: SourceActor,
  range: Range
): Promise<{ [number]: number[] }> {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  return sourceFront.getBreakpointPositionsCompressed(range);
}

async function getSourceActorBreakableLines({
  thread,
  actor,
}: SourceActor): Promise<Array<number>> {
  const sourceThreadFront = lookupThreadFront(thread);
  const sourceFront = sourceThreadFront.source({ actor });
  let actorLines = [];
  try {
    actorLines = await sourceFront.getBreakableLines();
  } catch (e) {
    // Handle backward compatibility
    if (
      e.message &&
      e.message.match(/does not recognize the packet type getBreakableLines/)
    ) {
      const pos = await sourceFront.getBreakpointPositionsCompressed();
      actorLines = Object.keys(pos).map(line => Number(line));
    } else if (!e.message || !e.message.match(/Connection closed/)) {
      throw e;
    }
  }

  return actorLines;
}

function getFrontByID(actorID: String) {
  return debuggerClient.getFrontByID(actorID);
}

const clientCommands = {
  autocomplete,
  blackBox,
  createObjectFront,
  loadObjectProperties,
  releaseActor,
  interrupt,
  pauseGrip,
  resume,
  stepIn,
  stepOut,
  stepOver,
  rewind,
  reverseStepOver,
  breakOnNext,
  sourceContents,
  getSourceForActor,
  getSourceActorBreakpointPositions,
  getSourceActorBreakableLines,
  hasBreakpoint,
  setBreakpoint,
  setXHRBreakpoint,
  removeXHRBreakpoint,
  addWatchpoint,
  removeWatchpoint,
  removeBreakpoint,
  evaluate,
  evaluateInFrame,
  evaluateExpressions,
  navigate,
  reload,
  getProperties,
  getFrameScopes,
  pauseOnExceptions,
  toggleEventLogging,
  fetchSources,
  checkIfAlreadyPaused,
  registerSourceActor,
  fetchThreads,
  getMainThread,
  sendPacket,
  setSkipPausing,
  setEventListenerBreakpoints,
  getEventListenerBreakpointTypes,
  detachWorkers,
  lookupTarget,
  getFrontByID,
};

export { setupCommands, clientCommands };
