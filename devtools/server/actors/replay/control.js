/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable spaced-comment, brace-style, indent-legacy, no-shadow */

"use strict";

// This file provides an interface which the ReplayDebugger uses to interact
// with a middleman's child recording and replaying processes. There can be
// several child processes in existence at once; this is largely hidden from the
// ReplayDebugger, and the position of each child is managed to provide a fast
// and stable experience when rewinding or running forward.

const CC = Components.Constructor;

// Create a sandbox with the resources we need. require() doesn't work here.
const sandbox = Cu.Sandbox(
  CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")()
);
Cu.evalInSandbox(
  "Components.utils.import('resource://gre/modules/jsdebugger.jsm');" +
    "Components.utils.import('resource://gre/modules/Services.jsm');" +
    "Components.utils.import('resource://devtools/shared/execution-point-utils.js');" +
    "Components.utils.import('resource://gre/modules/Timer.jsm');" +
    "addDebuggerToGlobal(this);",
  sandbox
);
const {
  RecordReplayControl,
  Services,
  pointEquals,
  pointToString,
  findClosestPoint,
  pointArrayIncludes,
  pointPrecedes,
  positionEquals,
  positionSubsumes,
  setInterval,
  setTimeout,
} = sandbox;

const InvalidCheckpointId = 0;
const FirstCheckpointId = 1;

// Application State Control
//
// This section describes the strategy used for managing child processes so that
// we can be responsive to user interactions. There is at most one recording
// child process, and one or more replaying child processes.
//
// The recording child cannot rewind: it only runs forward and adds new data to
// the recording. If we are paused or trying to pause at a place within the
// recording, then the recording child is also paused.
//
// To manage the replaying children, we identify a set of checkpoints that will
// be saved by some replaying child. The duration between saved checkpoints
// should be roughly equal, and they must be sufficiently closely spaced that
// any point in the recording can be quickly reached by some replaying child
// restoring the previous saved checkpoint and then running forward to the
// target point.
//
// As we identify the saved checkpoints, each is assigned to some replaying
// child, which is responsible for both saving that checkpoint and for scanning
// the contents of the recording from that checkpoint until the next saved
// checkpoint.
//
// When adding new data to the recording, replaying children will scan and save
// the regions and checkpoints they are responsible for, and will otherwise play
// forward as far as they can in the recording. We always want to have one or
// more replaying children that are at far end of the recording and able to
// start scanning/saving as the recording grows. In order to ensure this,
// consider the following:
//
// - Replaying must be faster than recording. While recording there is idle time
//   as we wait for user input, timers, etc. that is not present while
//   replaying. Suppose that replaying speed is F times the recording speed.
//   F must be less than one.
//
// - Scanning and saving a section of the recording is slower than recording.
//   Both of these have a lot of overhead, and suppose that scanning is S times
//   the recording speed. S will be more than one.
//
// - When there is more than one replaying child, each child can divide its time
//   between scanning/saving and simply replaying. We want its average speed to
//   match that of the recording. If there are N replaying children, each child
//   scans 1/N of the recording, so the average speed compared to the recording
//   is S/N + F*(N-1)/N. We want this term to be one or slightly less.
//
// For example, if F = 0.75 and S = 3, then replaying is 33% faster than
// recording, and scanning is three times slower than recording. If N = 4,
// the average speed is 3/4 + 0.75*3/4 = 1.31 times that of recording, which
// will cause the replaying processes to fall further and further behind the
// recording. If N = 12, the average speed is 3/12 + 0.75*11/12 = 0.94 times
// that of the recording, which will allow the replaying processes to keep up
// with the recording.
//
// Eventually we'll want to do this analysis dynamically to scale up or throttle
// back the number of active replaying processes. For now, though, we rely on
// a fixed number of replaying processes, and hope that it is enough.

////////////////////////////////////////////////////////////////////////////////
// Child Processes
////////////////////////////////////////////////////////////////////////////////

// Information about a child recording or replaying process.
function ChildProcess(id, recording) {
  this.id = id;

  // Whether this process is recording.
  this.recording = recording;

  // Whether this process is paused.
  this.paused = false;

  // The last point we paused at.
  this.lastPausePoint = null;

  // Last reported memory usage for this child.
  this.lastMemoryUsage = null;

  // All checkpoints which this process has saved or will save, which is a
  // subset of all the saved checkpoints.
  this.savedCheckpoints = new Set(recording ? [] : [FirstCheckpointId]);

  // All saved checkpoints whose region of the recording has been scanned or is
  // in the process of being scanned by this child.
  this.scannedCheckpoints = new Set();

  // All snapshots which this child has taken.
  this.snapshots = [];

  // Whether this child has diverged from the recording and cannot run forward.
  this.divergedFromRecording = false;

  // Whether this child has crashed and is unusable.
  this.crashed = false;

  // Any manifest which is currently being executed. Child processes initially
  // have a manifest to run forward to the first checkpoint.
  this.manifest = {
    onFinished: ({ point }) => {
      if (this == gMainChild) {
        getCheckpointInfo(FirstCheckpointId).point = point;
        Services.tm.dispatchToMainThread(
          recording ? maybeResumeRecording : setMainChild
        );
      } else {
        this.snapshots.push(checkpointExecutionPoint(FirstCheckpointId));
      }
    },
  };

  // The time the manifest was sent to the child.
  this.manifestSendTime = Date.now();

  // Any async manifest which this child has partially processed.
  this.asyncManifest = null;
}

ChildProcess.prototype = {
  // Get the execution point where this child is currently paused.
  pausePoint() {
    assert(this.paused);
    return this.lastPausePoint;
  },

  // Get the checkpoint where this child is currently paused.
  pauseCheckpoint() {
    const point = this.pausePoint();
    assert(!point.position);
    return point.checkpoint;
  },

  // Send a manifest to paused child to execute. The child unpauses while
  // executing the manifest, and pauses again when it finishes. Manifests have
  // the following properties:
  //
  // contents: The JSON object to send to the child describing the operation.
  // onFinished: A callback which is called after the manifest finishes with the
  //   manifest's result.
  // destination: An optional destination where the child will end up.
  // expectedDuration: Optional estimate of the time needed to run the manifest.
  sendManifest(manifest) {
    assert(!this.crashed);
    assert(this.paused);
    this.paused = false;
    this.manifest = manifest;
    this.manifestSendTime = Date.now();

    dumpv(`SendManifest #${this.id} ${stringify(manifest.contents)}`);
    RecordReplayControl.sendManifest(this.id, manifest.contents);

    // When sending a manifest to a replaying process, make sure we can detect
    // hangs in the process. A hanged replaying process will only be terminated
    // if we wait on it, so use a sufficiently long timeout and enter the wait
    // if the child hasn't completed its manifest by the deadline.
    if (this != gMainChild) {
      // Use the same 30 second wait which RecordReplayControl uses, so that the
      // hang will be noticed immediately if we start waiting.
      let waitDuration = 30 * 1000;
      if (manifest.expectedDuration) {
        waitDuration += manifest.expectedDuration;
      }
      setTimeout(() => {
        if (!this.crashed && this.manifest == manifest) {
          this.waitUntilPaused();
        }
      }, waitDuration);
    }
  },

  // Called when the child's current manifest finishes.
  manifestFinished(response) {
    assert(!this.paused);
    if (response) {
      if (response.point) {
        this.lastPausePoint = response.point;
      }
      if (response.memoryUsage) {
        this.lastMemoryUsage = response.memoryUsage;
      }
      if (response.exception) {
        ThrowError(response.exception);
      }
    }
    this.paused = true;
    this.manifest.onFinished(response);
    this.manifest = null;
    maybeDumpStatistics();

    if (this != gMainChild) {
      pokeChildSoon(this);
    }
  },

  // Block until this child is paused. If maybeCreateCheckpoint is specified
  // then a checkpoint is created if this child is recording, so that it pauses
  // quickly (otherwise it could sit indefinitely if there is no page activity).
  waitUntilPaused(maybeCreateCheckpoint) {
    if (this.paused) {
      return;
    }
    RecordReplayControl.waitUntilPaused(this.id, maybeCreateCheckpoint);
    assert(this.paused || this.crashed);
  },

  // Add a checkpoint for this child to save.
  addSavedCheckpoint(checkpoint) {
    dumpv(`AddSavedCheckpoint #${this.id} ${checkpoint}`);
    this.savedCheckpoints.add(checkpoint);
  },

  // Get the checkpoints which the child must save in the range [start, end].
  savedCheckpointsInRange(start, end) {
    const rv = [];
    for (let i = start; i <= end; i++) {
      if (this.savedCheckpoints.has(i)) {
        rv.push(i);
      }
    }
    return rv;
  },

  // Get the locations of snapshots for saved checkpoints which the child must
  // save when running forward to endpoint.
  getSnapshotsForSavedCheckpoints(endpoint) {
    assert(pointPrecedes(this.pausePoint(), endpoint));
    return this.savedCheckpointsInRange(
      this.pausePoint().checkpoint + 1,
      endpoint.checkpoint
    ).map(checkpointExecutionPoint);
  },

  // Get the point of the last snapshot which was taken.
  lastSnapshot() {
    return this.snapshots[this.snapshots.length - 1];
  },

  // Get an estimate of the amount of time required for this child to reach an
  // execution point.
  timeToReachPoint(point) {
    let startDelay = 0;
    let startPoint = this.lastPausePoint;
    if (!startPoint) {
      startPoint = checkpointExecutionPoint(FirstCheckpointId);
    }
    if (!this.paused) {
      if (this.manifest.expectedDuration) {
        const elapsed = Date.now() - this.manifestSendTime;
        if (elapsed < this.manifest.expectedDuration) {
          startDelay = this.manifest.expectedDuration - elapsed;
        }
      }
      if (this.manifest.destination) {
        startPoint = this.manifest.destination;
      }
    }
    let startCheckpoint;
    if (this.snapshots.length) {
      let snapshotIndex = this.snapshots.length - 1;
      while (pointPrecedes(point, this.snapshots[snapshotIndex])) {
        snapshotIndex--;
      }
      startCheckpoint = this.snapshots[snapshotIndex].checkpoint;
    } else {
      // Children which just started haven't taken snapshots.
      startCheckpoint = FirstCheckpointId;
    }
    return (
      startDelay + checkpointRangeDuration(startCheckpoint, point.checkpoint)
    );
  },
};

// Child which is always at the end of the recording. When there is a recording
// child this is it, and when we are replaying an old execution this is a
// replaying child that is unable to rewind and is used in the same way as the
// recording child.
let gMainChild;

// Replaying children available for exploring the interior of the recording,
// indexed by their ID.
const gReplayingChildren = [];

function lookupChild(id) {
  if (id == gMainChild.id) {
    return gMainChild;
  }
  return gReplayingChildren[id];
}

function closestChild(point) {
  let minChild = null,
    minTime = Infinity;
  for (const child of gReplayingChildren) {
    if (child) {
      const time = child.timeToReachPoint(point);
      if (time < minTime) {
        minChild = child;
        minTime = time;
      }
    }
  }
  return minChild;
}

// The singleton ReplayDebugger, or undefined if it doesn't exist.
let gDebugger;

////////////////////////////////////////////////////////////////////////////////
// Asynchronous Manifests
////////////////////////////////////////////////////////////////////////////////

// Asynchronous manifest worklists.
const gAsyncManifests = new Set();
const gAsyncManifestsLowPriority = new Set();

function asyncManifestWorklist(lowPriority) {
  return lowPriority ? gAsyncManifestsLowPriority : gAsyncManifests;
}

// Send a manifest asynchronously, returning a promise that resolves when the
// manifest has been finished. Async manifests have the following properties:
//
// shouldSkip: Callback invoked before sending the manifest. Returns true if the
//   manifest should not be sent, and the promise resolved immediately.
//
// contents: Callback invoked with the executing child when it is being sent.
//   Returns the contents to send to the child.
//
// onFinished: Callback invoked with the executing child and manifest response
//   after the manifest finishes.
//
// point: Optional point which the associated child must reach before sending
//   the manifest.
//
// snapshot: Optional point at which the associated child should take a snapshot
//   while heading to the point.
//
// scanCheckpoint: If the manifest relies on scan data, the saved checkpoint
//   whose range the child must have scanned. Such manifests do not have side
//   effects in the child, and can be sent to the active child.
//
// lowPriority: True if this manifest should be processed only after all other
//   manifests have been processed.
//
// destination: An optional destination where the child will end up.
//
// expectedDuration: Optional estimate of the time needed to run the manifest.
function sendAsyncManifest(manifest) {
  pokeChildrenSoon();
  return new Promise(resolve => {
    manifest.resolve = resolve;
    asyncManifestWorklist(manifest.lowPriority).add(manifest);
  });
}

// Pick the best async manifest for a child to process.
function pickAsyncManifest(child, lowPriority) {
  const worklist = asyncManifestWorklist(lowPriority);

  let best = null,
    bestTime = Infinity;
  for (const manifest of worklist) {
    // Prune any manifests that can be skipped.
    if (manifest.shouldSkip()) {
      manifest.resolve();
      worklist.delete(manifest);
      continue;
    }

    // Manifests relying on scan data can be handled by any child, at any point.
    // These are the best ones to pick.
    if (manifest.scanCheckpoint) {
      if (child.scannedCheckpoints.has(manifest.scanCheckpoint)) {
        assert(!manifest.point);
        best = manifest;
        break;
      } else {
        continue;
      }
    }

    // The active child cannot process other asynchronous manifests which don't
    // rely on scan data, as they can move the child or have other side effects.
    if (child == gActiveChild) {
      continue;
    }

    // Pick the manifest which requires the least amount of travel time.
    assert(manifest.point);
    const time = child.timeToReachPoint(manifest.point);
    if (time < bestTime) {
      best = manifest;
      bestTime = time;
    }
  }

  if (best) {
    worklist.delete(best);
  }

  return best;
}

function processAsyncManifest(child) {
  // If the child has partially processed a manifest, continue with it.
  let manifest = child.asyncManifest;
  child.asyncManifest = null;

  if (manifest && child == gActiveChild) {
    // After a child becomes the active child, it gives up on any in progress
    // async manifest it was processing.
    sendAsyncManifest(manifest);
    manifest = null;
  }

  if (!manifest) {
    manifest = pickAsyncManifest(child, /* lowPriority */ false);
    if (!manifest) {
      manifest = pickAsyncManifest(child, /* lowPriority */ true);
      if (!manifest) {
        return false;
      }
    }
  }

  child.asyncManifest = manifest;

  if (
    manifest.point &&
    maybeReachPoint(child, manifest.point, manifest.snapshot)
  ) {
    // The manifest has been partially processed.
    return true;
  }

  child.sendManifest({
    contents: manifest.contents(child),
    onFinished: data => {
      child.asyncManifest = null;
      manifest.onFinished(child, data);
      manifest.resolve();
    },
    destination: manifest.destination,
    expectedDuration: manifest.expectedDuration,
  });

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Application State
////////////////////////////////////////////////////////////////////////////////

// Any child the user is interacting with, which may be paused or not.
let gActiveChild = null;

// Information about each checkpoint, indexed by the checkpoint's id.
const gCheckpoints = [null];

function CheckpointInfo() {
  // The time taken to run from this checkpoint to the next one, excluding idle
  // time.
  this.duration = 0;

  // Execution point at the checkpoint.
  this.point = null;

  // Whether the checkpoint is saved.
  this.saved = false;

  // If the checkpoint is saved, the time it was assigned to a child.
  this.assignTime = null;

  // If the checkpoint is saved and scanned, the time it finished being scanned.
  this.scanTime = null;

  // If the checkpoint is saved and scanned, the duration of the scan.
  this.scanDuration = null;

  // If the checkpoint is saved, any debugger statement hits in its region.
  this.debuggerStatements = [];

  // If the checkpoint is saved, any events in its region.
  this.events = [];
}

function getCheckpointInfo(id) {
  while (id >= gCheckpoints.length) {
    gCheckpoints.push(new CheckpointInfo());
  }
  return gCheckpoints[id];
}

// How much execution time elapses between two checkpoints.
function checkpointRangeDuration(start, end) {
  let time = 0;
  for (let i = start; i < end; i++) {
    time += gCheckpoints[i].duration;
  }
  return time;
}

// How much execution time has elapsed since a checkpoint.
function timeSinceCheckpoint(id) {
  return checkpointRangeDuration(id, gCheckpoints.length);
}

// How much execution time is captured by a saved checkpoint.
function timeForSavedCheckpoint(id) {
  return checkpointRangeDuration(id, nextSavedCheckpoint(id));
}

// The checkpoint up to which the recording runs.
let gLastFlushCheckpoint = InvalidCheckpointId;

// How often we want to flush the recording.
const FlushMs = 0.5 * 1000;

// ID of the last replaying child we picked for saving a checkpoint.
let gLastPickedChildId = 0;

function addSavedCheckpoint(checkpoint) {
  getCheckpointInfo(checkpoint).saved = true;
  getCheckpointInfo(checkpoint).assignTime = Date.now();

  if (RecordReplayControl.canRewind()) {
    // Use a round robin approach when picking children for saving checkpoints.
    let child;
    while (true) {
      gLastPickedChildId = (gLastPickedChildId + 1) % gReplayingChildren.length;
      child = gReplayingChildren[gLastPickedChildId];
      if (child) {
        break;
      }
    }

    child.addSavedCheckpoint(checkpoint);
  }
}

function addCheckpoint(checkpoint, duration) {
  assert(!getCheckpointInfo(checkpoint).duration);
  getCheckpointInfo(checkpoint).duration = duration;
}

// Bring a child to the specified execution point, sending it one or more
// manifests if necessary. Returns true if the child has not reached the point
// yet but some progress was made, or false if the child is at the point.
// Snapshot specifies any point at which a snapshot should be taken.
function maybeReachPoint(child, endpoint, snapshot) {
  if (
    pointEquals(child.pausePoint(), endpoint) &&
    !child.divergedFromRecording
  ) {
    return false;
  }

  if (pointPrecedes(endpoint, child.pausePoint())) {
    restoreSnapshotPriorTo(endpoint);
    return true;
  }

  if (child.divergedFromRecording) {
    restoreSnapshotPriorTo(child.pausePoint());
    return true;
  }

  const snapshotPoints = child.getSnapshotsForSavedCheckpoints(endpoint);
  if (
    snapshot &&
    pointPrecedes(child.pausePoint(), snapshot) &&
    !pointArrayIncludes(snapshotPoints, snapshot)
  ) {
    snapshotPoints.push(snapshot);
  }

  child.sendManifest({
    contents: { kind: "runToPoint", endpoint, snapshotPoints },
    onFinished() {
      child.snapshots.push(...snapshotPoints);
    },
    destination: endpoint,
    expectedDuration: checkpointRangeDuration(
      child.pausePoint().checkpoint,
      endpoint.checkpoint
    ),
  });

  return true;

  // Send the child to its most recent saved checkpoint at or before target.
  function restoreSnapshotPriorTo(target) {
    let numSnapshots = 0;
    while (pointPrecedes(target, child.lastSnapshot())) {
      numSnapshots++;
      child.snapshots.pop();
    }
    child.sendManifest({
      contents: { kind: "restoreSnapshot", numSnapshots },
      onFinished({ restoredSnapshot }) {
        assert(restoredSnapshot);
        child.divergedFromRecording = false;
      },
      destination: child.lastSnapshot(),
    });
  }
}

function nextSavedCheckpoint(checkpoint) {
  assert(gCheckpoints[checkpoint].saved);
  // eslint-disable-next-line no-empty
  while (!gCheckpoints[++checkpoint].saved) {}
  return checkpoint;
}

function forSavedCheckpointsInRange(start, end, callback) {
  if (start == FirstCheckpointId && !gCheckpoints[start].saved) {
    return;
  }
  assert(gCheckpoints[start].saved);
  for (
    let checkpoint = start;
    checkpoint < end;
    checkpoint = nextSavedCheckpoint(checkpoint)
  ) {
    callback(checkpoint);
  }
}

function forAllSavedCheckpoints(callback) {
  forSavedCheckpointsInRange(FirstCheckpointId, gLastFlushCheckpoint, callback);
}

function getSavedCheckpoint(checkpoint) {
  while (!gCheckpoints[checkpoint].saved) {
    checkpoint--;
  }
  return checkpoint;
}

// Get the execution point to use for a checkpoint.
function checkpointExecutionPoint(checkpoint) {
  return gCheckpoints[checkpoint].point;
}

// Check to see if an idle replaying child can make any progress.
function pokeChild(child) {
  assert(child != gMainChild);

  if (!child.paused) {
    return;
  }

  if (processAsyncManifest(child)) {
    return;
  }

  if (child == gActiveChild) {
    sendActiveChildToPausePoint();
    return;
  }

  // If there is nothing to do, run forward to the end of the recording.
  if (gLastFlushCheckpoint) {
    maybeReachPoint(child, checkpointExecutionPoint(gLastFlushCheckpoint));
  }
}

function pokeChildSoon(child) {
  Services.tm.dispatchToMainThread(() => pokeChild(child));
}

let gPendingPokeChildren = false;

function pokeChildren() {
  gPendingPokeChildren = false;
  for (const child of gReplayingChildren) {
    if (child) {
      pokeChild(child);
    }
  }
}

function pokeChildrenSoon() {
  if (!gPendingPokeChildren) {
    Services.tm.dispatchToMainThread(() => pokeChildren());
    gPendingPokeChildren = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Search State
////////////////////////////////////////////////////////////////////////////////

// All currently installed breakpoints.
const gBreakpoints = [];

// Recording Scanning
//
// Scanning a section of the recording between two neighboring saved checkpoints
// allows the execution points for each script breakpoint position to be queried
// by sending a manifest to the child which performed the scan.

function findScanChild(checkpoint) {
  for (const child of gReplayingChildren) {
    if (child && child.scannedCheckpoints.has(checkpoint)) {
      return child;
    }
  }
  return null;
}

// Ensure the region for a saved checkpoint has been scanned by some child.
async function scanRecording(checkpoint) {
  assert(checkpoint < gLastFlushCheckpoint);

  const child = findScanChild(checkpoint);
  if (child) {
    return;
  }

  const endpoint = checkpointExecutionPoint(nextSavedCheckpoint(checkpoint));
  let snapshotPoints = null;
  await sendAsyncManifest({
    shouldSkip: () => !!findScanChild(checkpoint),
    contents(child) {
      child.scannedCheckpoints.add(checkpoint);
      snapshotPoints = child.getSnapshotsForSavedCheckpoints(endpoint);
      return {
        kind: "scanRecording",
        endpoint,
        snapshotPoints,
      };
    },
    onFinished(child, { duration }) {
      child.snapshots.push(...snapshotPoints);
      const info = getCheckpointInfo(checkpoint);
      if (!info.scanTime) {
        info.scanTime = Date.now();
        info.scanDuration = duration;
      }
      if (gDebugger) {
        gDebugger._callOnPositionChange();
      }
    },
    point: checkpointExecutionPoint(checkpoint),
    destination: endpoint,
    expectedDuration: checkpointRangeDuration(checkpoint, endpoint) * 5,
  });

  assert(findScanChild(checkpoint));
}

function unscannedRegions() {
  const result = [];

  function addRegion(startCheckpoint, endCheckpoint) {
    const start = checkpointExecutionPoint(startCheckpoint).progress;
    const end = checkpointExecutionPoint(endCheckpoint).progress;

    if (result.length && result[result.length - 1].end == start) {
      result[result.length - 1].end = end;
    } else {
      result.push({ start, end });
    }
  }

  forAllSavedCheckpoints(checkpoint => {
    if (!findScanChild(checkpoint)) {
      addRegion(checkpoint, nextSavedCheckpoint(checkpoint));
    }
  });

  const lastFlush = gLastFlushCheckpoint || FirstCheckpointId;
  if (lastFlush != gRecordingEndpoint) {
    addRegion(lastFlush, gMainChild.lastPausePoint.checkpoint);
  }

  return result;
}

// Map from saved checkpoints to information about breakpoint hits within the
// range of that checkpoint.
const gHitSearches = new Map();

// Only hits on script locations (Break and OnStep positions) can be found by
// scanning the recording.
function canFindHits(position) {
  return position.kind == "Break" || position.kind == "OnStep";
}

// Find all hits on the specified position between a saved checkpoint and the
// following saved checkpoint, using data from scanning the recording. This
// returns a promise that resolves with the resulting hits.
async function findHits(checkpoint, position) {
  assert(canFindHits(position));
  assert(gCheckpoints[checkpoint].saved);

  if (!gHitSearches.has(checkpoint)) {
    gHitSearches.set(checkpoint, []);
  }

  // Check if we already have the hits.
  let hits = findExisting();
  if (hits) {
    return hits;
  }

  await scanRecording(checkpoint);
  const endpoint = nextSavedCheckpoint(checkpoint);
  await sendAsyncManifest({
    shouldSkip: () => !!findExisting(),
    contents() {
      return {
        kind: "findHits",
        position,
        startpoint: checkpoint,
        endpoint,
      };
    },
    onFinished(_, hits) {
      if (!gHitSearches.has(checkpoint)) {
        gHitSearches.set(checkpoint, []);
      }
      const checkpointHits = gHitSearches.get(checkpoint);
      checkpointHits.push({ position, hits });
    },
    scanCheckpoint: checkpoint,
  });

  hits = findExisting();
  assert(hits);
  return hits;

  function findExisting() {
    const checkpointHits = gHitSearches.get(checkpoint);
    if (!checkpointHits) {
      return null;
    }
    const entry = checkpointHits.find(
      ({ position: existingPosition, hits }) => {
        return positionEquals(position, existingPosition);
      }
    );
    return entry ? entry.hits : null;
  }
}

// Asynchronously find all hits on a breakpoint's position.
async function findBreakpointHits(checkpoint, position) {
  if (position.kind == "Break") {
    findHits(checkpoint, position);
  }
}

// Frame Steps
//
// When the recording scanning is not sufficient to figure out where to stop
// when resuming, the steps for the currently paused frame can be fetched. This
// mainly helps with finding the targets for EnterFrame breakpoints used when
// stepping in, and will be used in the future to improve stepping performance.
//
// The steps for a frame are the list of execution points for breakpoint
// positions traversed when executing a particular script frame, from the
// initial EnterFrame to the final OnPop. The steps also include the EnterFrame
// execution points for any direct callees of the frame.

// Map from point strings to the steps which contain them.
const gFrameSteps = new Map();

// Map from frame entry point strings to the parent frame's entry point.
const gParentFrames = new Map();

// Find all the steps in the frame which point is part of. This returns a
// promise that resolves with the steps that were found.
async function findFrameSteps(point) {
  if (!point.position) {
    return null;
  }

  assert(
    point.position.kind == "EnterFrame" ||
      point.position.kind == "OnStep" ||
      point.position.kind == "OnPop"
  );

  let steps = findExisting();
  if (steps) {
    return steps;
  }

  // Gather information which the child which did the scan can use to figure out
  // the different frame steps.
  const info = gControl.sendRequestMainChild({
    type: "frameStepsInfo",
    script: point.position.script,
  });

  const checkpoint = getSavedCheckpoint(point.checkpoint);
  await scanRecording(checkpoint);
  await sendAsyncManifest({
    shouldSkip: () => !!findExisting(),
    contents: () => ({ kind: "findFrameSteps", targetPoint: point, ...info }),
    onFinished: (_, steps) => {
      for (const p of steps) {
        if (p.position.frameIndex == point.position.frameIndex) {
          gFrameSteps.set(pointToString(p), steps);
        } else {
          assert(p.position.kind == "EnterFrame");
          gParentFrames.set(pointToString(p), steps[0]);
        }
      }
    },
    scanCheckpoint: checkpoint,
  });

  steps = findExisting();
  assert(steps);
  return steps;

  function findExisting() {
    return gFrameSteps.get(pointToString(point));
  }
}

async function findParentFrameEntryPoint(point) {
  assert(point.position.kind == "EnterFrame");
  assert(point.position.frameIndex > 0);

  let parentPoint = findExisting();
  if (parentPoint) {
    return parentPoint;
  }

  const checkpoint = getSavedCheckpoint(point.checkpoint);
  await scanRecording(checkpoint);
  await sendAsyncManifest({
    shouldSkip: () => !!findExisting(),
    contents: () => ({ kind: "findParentFrameEntryPoint", point }),
    onFinished: (_, { parentPoint }) => {
      gParentFrames.set(pointToString(point), parentPoint);
    },
    scanCheckpoint: checkpoint,
  });

  parentPoint = findExisting();
  assert(parentPoint);
  return parentPoint;

  function findExisting() {
    return gParentFrames.get(pointToString(point));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Pause Data
////////////////////////////////////////////////////////////////////////////////

const gPauseData = new Map();
const gQueuedPauseData = new Set();

// Cached points indicate messages where we have gathered pause data. These are
// shown differently in the UI.
const gCachedPoints = new Map();

async function queuePauseData({
  point,
  snapshot,
  trackCached,
  shouldSkip: shouldSkipCallback,
}) {
  if (gQueuedPauseData.has(pointToString(point))) {
    return;
  }
  gQueuedPauseData.add(pointToString(point));

  await waitForFlushed(point.checkpoint);

  sendAsyncManifest({
    shouldSkip() {
      if (maybeGetPauseData(point)) {
        return true;
      }

      // If there is a logpoint at a position we will see a breakpoint as well.
      // When the logpoint's text is resolved at this point then the pause data
      // will be fetched as well.
      if (
        point.position &&
        gLogpoints.some(({ position }) =>
          positionSubsumes(position, point.position)
        )
      ) {
        return true;
      }

      return shouldSkipCallback && shouldSkipCallback();
    },
    contents() {
      return { kind: "getPauseData" };
    },
    onFinished(child, data) {
      if (!data.restoredSnapshot) {
        addPauseData(point, data, trackCached);
        child.divergedFromRecording = true;
      }
    },
    point,
    snapshot,
    expectedDuration: 250,
    lowPriority: true,
  });
}

function addPauseData(point, data, trackCached) {
  if (data.paintData) {
    // Atomize paint data strings to ensure that we don't store redundant
    // strings for execution points with the same paint data.
    data.paintData = RecordReplayControl.atomize(data.paintData);
  }
  gPauseData.set(pointToString(point), data);

  if (trackCached) {
    gCachedPoints.set(pointToString(point), point);
    if (gDebugger) {
      gDebugger._callOnPositionChange();
    }
  }
}

function maybeGetPauseData(point) {
  return gPauseData.get(pointToString(point));
}

function cachedPoints() {
  return [...gCachedPoints.values()].map(point => point.progress);
}

////////////////////////////////////////////////////////////////////////////////
// Pause State
////////////////////////////////////////////////////////////////////////////////

// The pause mode classifies the current state of the debugger.
const PauseModes = {
  // The main child is the active child, and is either paused or actively
  // recording. gPausePoint is the last point the main child reached.
  RUNNING: "RUNNING",

  // gActiveChild is a replaying child paused at gPausePoint.
  PAUSED: "PAUSED",

  // gActiveChild is a replaying child being taken to gPausePoint. The debugger
  // is considered to be paused, but interacting with the child must wait until
  // it arrives.
  ARRIVING: "ARRIVING",

  // gActiveChild is null, and we are looking for the last breakpoint hit prior
  // to or following gPausePoint, at which we will pause.
  RESUMING_BACKWARD: "RESUMING_BACKWARD",
  RESUMING_FORWARD: "RESUMING_FORWARD",
};

// Current pause mode.
let gPauseMode = PauseModes.RUNNING;

// In PAUSED or ARRIVING modes, the point we are paused at or sending the active
// child to.
let gPausePoint = null;

// In PAUSED mode, any debugger requests that have been sent to the child.
// In ARRIVING mode, the requests must be sent once the child arrives.
const gDebuggerRequests = [];

function addDebuggerRequest(request) {
  gDebuggerRequests.push({
    request,
    stack: Error().stack,
  });
}

function setPauseState(mode, point, child) {
  assert(mode);
  const idString = child ? ` #${child.id}` : "";
  dumpv(`SetPauseState ${mode} ${JSON.stringify(point)}${idString}`);

  gPauseMode = mode;
  gPausePoint = point;
  gActiveChild = child;

  if (mode == PauseModes.ARRIVING) {
    simulateNearbyNavigation();
  }

  pokeChildrenSoon();
}

// Mark the debugger as paused, and asynchronously send a child to the pause
// point.
function setReplayingPauseTarget(point) {
  assert(!gDebuggerRequests.length);
  setPauseState(PauseModes.ARRIVING, point, closestChild(point.checkpoint));

  gDebugger._onPause();

  findFrameSteps(point);
}

function sendActiveChildToPausePoint() {
  assert(gActiveChild.paused);

  switch (gPauseMode) {
    case PauseModes.PAUSED:
      assert(pointEquals(gActiveChild.pausePoint(), gPausePoint));
      return;

    case PauseModes.ARRIVING:
      if (pointEquals(gActiveChild.pausePoint(), gPausePoint)) {
        setPauseState(PauseModes.PAUSED, gPausePoint, gActiveChild);

        // Send any debugger requests the child is considered to have received.
        if (gDebuggerRequests.length) {
          const child = gActiveChild;
          child.sendManifest({
            contents: {
              kind: "batchDebuggerRequest",
              requests: gDebuggerRequests.map(r => r.request),
            },
            onFinished(finishData) {
              assert(!finishData.restoredSnapshot);
              if (finishData.divergedFromRecording) {
                child.divergedFromRecording = true;
              }
            },
          });
        }
      } else {
        maybeReachPoint(gActiveChild, gPausePoint);
      }
      return;

    default:
      ThrowError(`Unexpected pause mode: ${gPauseMode}`);
  }
}

function waitUntilPauseFinishes() {
  assert(gActiveChild);

  if (gActiveChild == gMainChild) {
    gActiveChild.waitUntilPaused(true);
    return;
  }

  while (gPauseMode != PauseModes.PAUSED) {
    gActiveChild.waitUntilPaused();
    pokeChild(gActiveChild);
  }

  gActiveChild.waitUntilPaused();
}

// Synchronously send a child to the specific point and pause.
function pauseReplayingChild(child, point) {
  setPauseState(PauseModes.ARRIVING, point, child);
  waitUntilPauseFinishes();
}

// Find the point where the debugger should pause when running forward or
// backward from a point and using a given set of breakpoints. Returns null if
// there is no point to pause at before hitting the beginning or end of the
// recording.
async function resumeTarget(point, forward, breakpoints) {
  let startCheckpoint = point.checkpoint;
  if (!forward && !point.position) {
    startCheckpoint--;
    if (startCheckpoint == InvalidCheckpointId) {
      return null;
    }
  }
  startCheckpoint = getSavedCheckpoint(startCheckpoint);

  let checkpoint = startCheckpoint;
  for (; ; forward ? checkpoint++ : checkpoint--) {
    if ([InvalidCheckpointId, gLastFlushCheckpoint].includes(checkpoint)) {
      return null;
    }

    if (!gCheckpoints[checkpoint].saved) {
      continue;
    }

    const hits = [];

    // Find any breakpoint hits in this region of the recording.
    for (const bp of breakpoints) {
      if (canFindHits(bp)) {
        const bphits = await findHits(checkpoint, bp);
        hits.push(...bphits);
      }
    }

    // When there are stepping breakpoints, look for breakpoint hits in the
    // steps for the current frame.
    if (
      checkpoint == startCheckpoint &&
      gBreakpoints.some(bp => bp.kind == "EnterFrame" || bp.kind == "OnPop")
    ) {
      const steps = await findFrameSteps(point);
      hits.push(
        ...steps.filter(p => {
          return breakpoints.some(bp => positionSubsumes(bp, p.position));
        })
      );
    }

    // Always pause at debugger statements, as if they are breakpoint hits.
    hits.push(...getCheckpointInfo(checkpoint).debuggerStatements);

    const hit = findClosestPoint(
      hits,
      gPausePoint,
      /* before */ !forward,
      /* inclusive */ false
    );
    if (hit) {
      return hit;
    }
  }
}

async function finishResume() {
  assert(
    gPauseMode == PauseModes.RESUMING_FORWARD ||
      gPauseMode == PauseModes.RESUMING_BACKWARD
  );
  const forward = gPauseMode == PauseModes.RESUMING_FORWARD;

  const point = await resumeTarget(gPausePoint, forward, gBreakpoints);
  if (point) {
    // We found a point to pause at.
    setReplayingPauseTarget(point);
  } else if (forward) {
    // We searched the entire space forward to the end of the recording and
    // didn't find any breakpoint hits, so resume recording.
    assert(forward);
    RecordReplayControl.restoreMainGraphics();
    setPauseState(PauseModes.RUNNING, gMainChild.pausePoint(), gMainChild);
    gDebugger._callOnPositionChange();
    maybeResumeRecording();
  } else {
    // We searched backward to the beginning of the recording, so restore the
    // first checkpoint.
    assert(!forward);
    setReplayingPauseTarget(checkpointExecutionPoint(FirstCheckpointId));
  }
}

// Unpause the active child and asynchronously pause at the next or previous
// breakpoint hit.
function resume(forward) {
  gDebuggerRequests.length = 0;
  if (gActiveChild.recording) {
    if (forward) {
      maybeResumeRecording();
      return;
    }
    ensureFlushed();
  }
  if (
    gPausePoint.checkpoint == FirstCheckpointId &&
    !gPausePoint.position &&
    !forward
  ) {
    gDebugger._hitRecordingBoundary();
    return;
  }
  setPauseState(
    forward ? PauseModes.RESUMING_FORWARD : PauseModes.RESUMING_BACKWARD,
    gPausePoint,
    null
  );
  finishResume();
  pokeChildren();
}

// Synchronously bring the active child to the specified execution point.
function timeWarp(point) {
  gDebuggerRequests.length = 0;
  setReplayingPauseTarget(point);
  Services.cpmm.sendAsyncMessage("TimeWarpFinished");
}

////////////////////////////////////////////////////////////////////////////////
// Crash Recovery
////////////////////////////////////////////////////////////////////////////////

// The maximum number of crashes which we can recover from.
const MaxCrashes = 4;

// How many child processes have crashed.
let gNumCrashes = 0;

// eslint-disable-next-line no-unused-vars
function ChildCrashed(id) {
  dumpv(`Child Crashed: ${id}`);

  // In principle we can recover when any replaying child process crashes.
  // For simplicity, there are some cases where we don't yet try to recover if
  // a replaying process crashes.
  //
  // - It is the main child, and running forward through the recording. While it
  //   could crash here just as easily as any other replaying process, any crash
  //   will happen early on and won't interrupt a long-running debugger session.
  //
  // - It is the active child, and is paused at gPausePoint. It must have
  //   crashed while processing a debugger request, which is unlikely.
  const child = gReplayingChildren[id];
  if (
    !child ||
    !child.manifest ||
    (child == gActiveChild && gPauseMode == PauseModes.PAUSED)
  ) {
    ThrowError("Cannot recover from crashed child");
  }

  if (++gNumCrashes > MaxCrashes) {
    ThrowError("Too many crashes");
  }

  delete gReplayingChildren[id];
  child.crashed = true;

  // Spawn a new child to replace the one which just crashed.
  const newChild = spawnReplayingChild();
  pokeChildSoon(newChild);

  // The new child should save the same checkpoints as the old one.
  for (const checkpoint of child.savedCheckpoints) {
    newChild.addSavedCheckpoint(checkpoint);
  }

  // Any regions the old child scanned need to be rescanned.
  for (const checkpoint of child.scannedCheckpoints) {
    scanRecording(checkpoint);
  }

  // Requeue any async manifest the child was processing.
  if (child.asyncManifest) {
    sendAsyncManifest(child.asyncManifest);
  }

  // If the active child crashed while heading to the pause point, pick another
  // child to head to the pause point.
  if (child == gActiveChild) {
    assert(gPauseMode == PauseModes.ARRIVING);
    gActiveChild = closestChild(gPausePoint.checkpoint);
    pokeChildSoon(gActiveChild);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Simulated Navigation
////////////////////////////////////////////////////////////////////////////////

// When the user is paused somewhere in the recording, we want to obtain pause
// data for points which they can get to via the UI. This includes all messages
// on the timeline (including those for logpoints), breakpoints that can be
// reached by rewinding and resuming, and points that can be reached by
// stepping. In the latter two cases, we only want to queue up the pause data
// for points that are close to the current pause point, so that we don't waste
// time and resources getting pause data that isn't immediately needed.
// We handle the latter by simulating the results of user interactions to
// visit nearby breakpoints and steps, and queueing up the pause data for those
// points. The simulation is approximate, as stepping behavior in particular
// depends on information known to the thread actor which isn't available here.
//
// When the user navigates through the recording, these simulations are repeated
// to queue up pause data at new points. Ideally, we will capture the pause data
// quickly enough that it will be immediately available when the user pauses at
// a new location, so that they don't experience loading hiccups.

// Get the pause data for nearby breakpoint hits, ignoring steps.
async function simulateBreakpointNavigation(point, forward, count) {
  if (!count) {
    return;
  }

  const breakpoints = gBreakpoints.filter(bp => bp.kind == "Break");
  const next = await resumeTarget(point, forward, breakpoints);
  if (next) {
    queuePauseData({ point: next });
    simulateBreakpointNavigation(next, forward, count - 1);
  }
}

async function findFrameEntryPoint(point) {
  // We never want the debugger to stop at EnterFrame points corresponding to
  // when a frame was pushed on the stack. Instead, find the first point in the
  // frame which is a breakpoint site.
  assert(point.position.kind == "EnterFrame");
  const steps = await findFrameSteps(point);
  assert(pointEquals(steps[0], point));
  return steps[1];
}

async function simulateSteppingNavigation(point, count, frameCount, last) {
  if (!count || !point.position) {
    return;
  }
  const { script } = point.position;
  const dbgScript = gDebugger._getScript(script);

  const steps = await findFrameSteps(point);
  const pointIndex = steps.findIndex(p => pointEquals(p, point));

  if (last != "reverseStepOver") {
    for (let i = pointIndex + 1; i < steps.length; i++) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        queuePauseData({ point: p, snapshot: steps[0] });
        simulateSteppingNavigation(p, count - 1, frameCount, "stepOver");
        break;
      }
    }
  }

  if (last != "stepOver") {
    for (let i = pointIndex - 1; i >= 1; i--) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        queuePauseData({ point: p, snapshot: steps[0] });
        simulateSteppingNavigation(p, count - 1, frameCount, "reverseStepOver");
        break;
      }
    }
  }

  if (frameCount) {
    for (let i = pointIndex + 1; i < steps.length; i++) {
      const p = steps[i];
      if (isStepOverTarget(p)) {
        break;
      }
      if (p.position.script != script) {
        // Currently, the debugger will stop at the EnterFrame site and then run
        // forward to the first breakpoint site before pausing. We need pause
        // data from both points, unfortunately.
        queuePauseData({ point: p, snapshot: steps[0] });

        const np = await findFrameEntryPoint(p);
        queuePauseData({ point: np, snapshot: steps[0] });
        findHits(getSavedCheckpoint(point.checkpoint), np.position);
        simulateSteppingNavigation(np, count - 1, frameCount - 1, "stepIn");
        break;
      }
    }
  }

  if (
    frameCount &&
    last != "stepOver" &&
    last != "reverseStepOver" &&
    point.position.frameIndex
  ) {
    // The debugger will stop at the OnPop for the frame before finding a place
    // in the parent frame to pause at.
    queuePauseData({ point: steps[steps.length - 1], snapshot: steps[0] });

    const parentEntryPoint = await findParentFrameEntryPoint(steps[0]);
    const parentSteps = await findFrameSteps(parentEntryPoint);
    for (let i = 0; i < parentSteps.length; i++) {
      const p = parentSteps[i];
      if (pointPrecedes(point, p)) {
        // When stepping out we will stop at the next breakpoint site,
        // and do not need a point that is a stepping target.
        queuePauseData({ point: p, snapshot: parentSteps[0] });
        findHits(getSavedCheckpoint(point.checkpoint), p.position);
        simulateSteppingNavigation(p, count - 1, frameCount - 1, "stepOut");
        break;
      }
    }
  }

  function isStepOverTarget(p) {
    const { kind, offset } = p.position;
    return (
      kind == "OnPop" ||
      (kind == "OnStep" && dbgScript.getOffsetMetadata(offset).isStepStart)
    );
  }
}

function simulateNearbyNavigation() {
  // How many breakpoint hit navigations are simulated on either side of the
  // pause point.
  const numBreakpointHits = 2;

  // Maximum number of steps to take when simulating nearby steps.
  const numSteps = 4;

  // Maximum number of times to change frames when simulating nearby steps.
  const numChangeFrames = 2;

  simulateBreakpointNavigation(gPausePoint, true, numBreakpointHits);
  simulateBreakpointNavigation(gPausePoint, false, numBreakpointHits);
  simulateSteppingNavigation(gPausePoint, numSteps, numChangeFrames);
}

////////////////////////////////////////////////////////////////////////////////
// Logpoints
////////////////////////////////////////////////////////////////////////////////

// All installed logpoints. Logpoints are given to us by the debugger, after
// which we need to asynchronously send a child to every point where the
// logpoint's position is reached, evaluate code there and invoke the callback
// associated with the logpoint.
const gLogpoints = [];

async function evaluateLogpoint({ point, text, condition, callback }) {
  assert(point);
  if (!condition) {
    callback(point, ["Loading..."]);
  }
  let skipPauseData = false;
  const manifest = {
    shouldSkip: () => false,
    contents() {
      return { kind: "hitLogpoint", text, condition, skipPauseData };
    },
    onFinished(child, { pauseData, result, resultData, restoredSnapshot }) {
      if (restoredSnapshot) {
        if (!skipPauseData) {
          // Gathering pause data sometimes triggers a snapshot restore.
          skipPauseData = true;
          sendAsyncManifest(manifest);
        } else {
          callback(point, ["Recording divergence evaluating logpoint"]);
        }
      } else {
        if (result) {
          if (!skipPauseData) {
            addPauseData(point, pauseData, /* trackCached */ true);
          }
          callback(point, result, resultData);
        }
        child.divergedFromRecording = true;
      }
    },
    point,
    expectedDuration: 250,
    lowPriority: true,
  };
  sendAsyncManifest(manifest);
}

// Asynchronously invoke a logpoint's callback with all results from hitting
// the logpoint in the range of the recording covered by checkpoint.
async function findLogpointHits(
  checkpoint,
  { position, text, condition, callback }
) {
  const hits = await findHits(checkpoint, position);
  for (const point of hits) {
    evaluateLogpoint({ point, text, condition, callback });
  }
}

////////////////////////////////////////////////////////////////////////////////
// Event Breakpoints
////////////////////////////////////////////////////////////////////////////////

// Event kinds which will be logged. For now this set can only grow, as we don't
// have a way to remove old event logpoints from the client.
const gLoggedEvents = [];

const gEventFrameEntryPoints = new Map();

async function findEventFrameEntry(checkpoint, progress) {
  if (gEventFrameEntryPoints.has(progress)) {
    return gEventFrameEntryPoints.get(progress);
  }

  const savedCheckpoint = getSavedCheckpoint(checkpoint);
  await scanRecording(savedCheckpoint);
  await sendAsyncManifest({
    shouldSkip: () => gEventFrameEntryPoints.has(progress),
    contents: () => ({ kind: "findEventFrameEntry", checkpoint, progress }),
    onFinished: (_, { rv }) => gEventFrameEntryPoints.set(progress, rv),
    scanCheckpoint: savedCheckpoint,
  });

  const point = gEventFrameEntryPoints.get(progress);
  if (!point) {
    return null;
  }

  return findFrameEntryPoint(point);
}

async function findEventLogpointHits(checkpoint, event, callback) {
  for (const info of getCheckpointInfo(checkpoint).events) {
    if (info.event == event) {
      const point = await findEventFrameEntry(info.checkpoint, info.progress);
      if (point) {
        evaluateLogpoint({ point, text: "arguments[0]", callback });
      }
    }
  }
}

function setActiveEventBreakpoints(events, callback) {
  dumpv(`SetActiveEventBreakpoints ${JSON.stringify(events)}`);

  for (const event of events) {
    if (gLoggedEvents.some(info => info.event == event)) {
      continue;
    }
    gLoggedEvents.push({ event, callback });
    forAllSavedCheckpoints(checkpoint =>
      findEventLogpointHits(checkpoint, event, callback)
    );
  }
}

////////////////////////////////////////////////////////////////////////////////
// Saving Recordings
////////////////////////////////////////////////////////////////////////////////

// Resume manifests are sent when the main child is sent forward through the
// recording. Update state according to new data produced by the resume.
function handleResumeManifestResponse({
  point,
  duration,
  consoleMessages,
  scripts,
  debuggerStatements,
  events,
}) {
  if (!point.position) {
    addCheckpoint(point.checkpoint - 1, duration);
    getCheckpointInfo(point.checkpoint).point = point;
  }

  if (gDebugger) {
    consoleMessages.forEach(msg => {
      gDebugger._newConsoleMessage(msg);
    });
  }

  if (gDebugger) {
    scripts.forEach(script => gDebugger._onNewScript(script));
  }

  consoleMessages.forEach(msg => {
    if (msg.executionPoint) {
      queuePauseData({ point: msg.executionPoint, trackCached: true });
    }
  });

  const savedCheckpoint = getSavedCheckpoint(
    point.position ? point.checkpoint : point.checkpoint - 1
  );

  for (const point of debuggerStatements) {
    getCheckpointInfo(savedCheckpoint).debuggerStatements.push(point);
  }

  for (const event of events) {
    getCheckpointInfo(savedCheckpoint).events.push(event);
  }

  // In repaint stress mode, the child process creates a checkpoint before every
  // paint. By gathering the pause data at these checkpoints, we will perform a
  // repaint at all of these checkpoints, ensuring that all the normal paints
  // can be repainted.
  if (RecordReplayControl.inRepaintStressMode()) {
    queuePauseData({ point });
  }
}

// If necessary, continue executing in the main child.
function maybeResumeRecording() {
  if (gActiveChild != gMainChild) {
    return;
  }

  if (
    !gLastFlushCheckpoint ||
    timeSinceCheckpoint(gLastFlushCheckpoint) >= FlushMs
  ) {
    ensureFlushed();
  }

  const checkpoint = gMainChild.pausePoint().checkpoint;
  if (!gMainChild.recording && checkpoint == gRecordingEndpoint) {
    ensureFlushed();
    Services.cpmm.sendAsyncMessage("HitRecordingEndpoint");
    if (gDebugger) {
      gDebugger._hitRecordingBoundary();
    }
    return;
  }
  gMainChild.sendManifest({
    contents: {
      kind: "resume",
      breakpoints: gBreakpoints,
      pauseOnDebuggerStatement: true,
    },
    onFinished(response) {
      handleResumeManifestResponse(response);

      gPausePoint = gMainChild.pausePoint();
      if (gDebugger) {
        gDebugger._onPause();
      } else {
        Services.tm.dispatchToMainThread(maybeResumeRecording);
      }
    },
  });
}

// Resolve callbacks for any promises waiting on the recording to be flushed.
const gFlushWaiters = [];

function waitForFlushed(checkpoint) {
  if (checkpoint < gLastFlushCheckpoint) {
    return undefined;
  }
  return new Promise(resolve => {
    gFlushWaiters.push(resolve);
  });
}

let gLastFlushTime = Date.now();

// If necessary, synchronously flush the recording to disk.
function ensureFlushed() {
  gMainChild.waitUntilPaused(true);

  gLastFlushTime = Date.now();

  if (gLastFlushCheckpoint == gMainChild.pauseCheckpoint()) {
    return;
  }

  if (gMainChild.recording) {
    gMainChild.sendManifest({
      contents: { kind: "flushRecording" },
      onFinished() {},
    });
    gMainChild.waitUntilPaused();
  }

  const oldFlushCheckpoint = gLastFlushCheckpoint || FirstCheckpointId;
  gLastFlushCheckpoint = gMainChild.pauseCheckpoint();

  // We now have a usable recording for replaying children, so spawn them if
  // necessary.
  if (gReplayingChildren.length == 0) {
    spawnReplayingChildren();
  }

  // Checkpoints where the recording was flushed to disk are saved. This allows
  // the recording to be scanned as soon as it has been flushed.
  addSavedCheckpoint(gLastFlushCheckpoint);

  // Flushing creates a new region of the recording for replaying children
  // to scan.
  forSavedCheckpointsInRange(
    oldFlushCheckpoint,
    gLastFlushCheckpoint,
    checkpoint => {
      scanRecording(checkpoint);

      // Scan for breakpoint and logpoint hits in this new region.
      gBreakpoints.forEach(position =>
        findBreakpointHits(checkpoint, position)
      );
      gLogpoints.forEach(logpoint => findLogpointHits(checkpoint, logpoint));
      for (const { event, callback } of gLoggedEvents) {
        findEventLogpointHits(checkpoint, event, callback);
      }
    }
  );

  for (const waiter of gFlushWaiters) {
    waiter();
  }
  gFlushWaiters.length = 0;

  pokeChildren();
}

const CheckFlushMs = 1000;

// Periodically make sure the recording is flushed. If the tab is sitting
// idle we still want to keep the recording up to date.
setInterval(() => {
  const elapsed = Date.now() - gLastFlushTime;
  if (
    elapsed > CheckFlushMs &&
    gMainChild.lastPausePoint &&
    gMainChild.lastPausePoint.checkpoint != gLastFlushCheckpoint
  ) {
    ensureFlushed();
  }
}, CheckFlushMs);

// eslint-disable-next-line no-unused-vars
function BeforeSaveRecording() {
  if (gActiveChild == gMainChild) {
    // The recording might not be up to date, ensure it flushes after pausing.
    ensureFlushed();
  }
}

// eslint-disable-next-line no-unused-vars
function AfterSaveRecording() {
  Services.cpmm.sendAsyncMessage("SaveRecordingFinished");
}

let gRecordingEndpoint;

function setMainChild() {
  assert(!gMainChild.recording);

  gMainChild.sendManifest({
    contents: { kind: "setMainChild" },
    onFinished({ endpoint }) {
      gRecordingEndpoint = endpoint;
      Services.tm.dispatchToMainThread(maybeResumeRecording);
    },
  });
}

////////////////////////////////////////////////////////////////////////////////
// Child Management
////////////////////////////////////////////////////////////////////////////////

function spawnReplayingChild() {
  const id = RecordReplayControl.spawnReplayingChild();
  const child = new ChildProcess(id, false);
  gReplayingChildren[id] = child;
  return child;
}

// How many replaying children to spawn. This should be a pref instead...
const NumReplayingChildren = 4;

function spawnReplayingChildren() {
  if (RecordReplayControl.canRewind()) {
    for (let i = 0; i < NumReplayingChildren; i++) {
      spawnReplayingChild();
    }
  }
  addSavedCheckpoint(FirstCheckpointId);
}

// eslint-disable-next-line no-unused-vars
function Initialize(recordingChildId) {
  try {
    if (recordingChildId != undefined) {
      gMainChild = new ChildProcess(recordingChildId, true);
    } else {
      // If there is no recording child, we have now initialized enough state
      // that we can start spawning replaying children.
      const id = RecordReplayControl.spawnReplayingChild();
      gMainChild = new ChildProcess(id, false);
      spawnReplayingChildren();
    }
    gActiveChild = gMainChild;
    return gControl;
  } catch (e) {
    dump(`ERROR: Initialize threw exception: ${e}\n`);
  }
}

// eslint-disable-next-line no-unused-vars
function ManifestFinished(id, response) {
  try {
    dumpv(`ManifestFinished #${id} ${stringify(response)}`);
    const child = lookupChild(id);
    if (child) {
      child.manifestFinished(response);
    } else {
      // Ignore messages from child processes that we have marked as crashed.
    }
  } catch (e) {
    dump(`ERROR: ManifestFinished threw exception: ${e} ${e.stack}\n`);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Debugger Operations
////////////////////////////////////////////////////////////////////////////////

// From the debugger's perspective, there is a single target to interact with,
// represented by gActiveChild. The details of the various children the control
// system is managing are hidden away. This object describes the interface which
// the debugger uses to access the control system.
const gControl = {
  // Get the current point where the active child is paused, or null.
  pausePoint() {
    if (gActiveChild && gActiveChild == gMainChild) {
      return gActiveChild.paused ? gActiveChild.pausePoint() : null;
    }
    if (gPauseMode == PauseModes.PAUSED || gPauseMode == PauseModes.ARRIVING) {
      return gPausePoint;
    }
    return null;
  },

  lastPausePoint() {
    return gPausePoint;
  },

  findFrameSteps(point) {
    return findFrameSteps(point);
  },

  // Return whether the active child is currently recording.
  childIsRecording() {
    return gActiveChild && gActiveChild.recording;
  },

  // Ensure the active child is paused.
  waitUntilPaused() {
    // The debugger should not use this method while we are actively resuming.
    assert(gActiveChild);

    if (gActiveChild == gMainChild) {
      gActiveChild.waitUntilPaused(true);
    }
  },

  // Add a breakpoint where the active child should pause while resuming.
  addBreakpoint(position) {
    dumpv(`AddBreakpoint ${JSON.stringify(position)}`);
    gBreakpoints.push(position);

    // Start searching for breakpoint hits in the recording immediately.
    if (canFindHits(position)) {
      forAllSavedCheckpoints(checkpoint =>
        findBreakpointHits(checkpoint, position)
      );
    }

    if (gActiveChild == gMainChild) {
      // The recording child will update its breakpoints when it reaches the
      // next checkpoint, so force it to create a checkpoint now.
      gActiveChild.waitUntilPaused(true);
    }

    simulateNearbyNavigation();
  },

  // Clear all installed breakpoints.
  clearBreakpoints() {
    dumpv(`ClearBreakpoints`);
    gBreakpoints.length = 0;

    if (gActiveChild == gMainChild) {
      // As for addBreakpoint(), update the active breakpoints in the recording
      // child immediately.
      gActiveChild.waitUntilPaused(true);
    }
  },

  // Get the last known point in the recording.
  recordingEndpoint() {
    return gMainChild.lastPausePoint;
  },

  // If the active child is currently recording, switch to a replaying one if
  // possible.
  maybeSwitchToReplayingChild() {
    assert(gControl.pausePoint());
    if (gActiveChild == gMainChild && RecordReplayControl.canRewind()) {
      const point = gActiveChild.pausePoint();

      if (point.position) {
        // We can only flush the recording at checkpoints, so we need to send the
        // main child forward and pause/flush ASAP.
        gMainChild.sendManifest({
          contents: {
            kind: "resume",
            breakpoints: [],
            pauseOnDebuggerStatement: false,
          },
          onFinished(response) {
            handleResumeManifestResponse(response);
          },
        });
        gMainChild.waitUntilPaused(true);
      }

      ensureFlushed();
      const child = closestChild(point);
      pauseReplayingChild(child, point);
    }
  },

  // Synchronously send a debugger request to a paused active child, returning
  // the response.
  sendRequest(request) {
    waitUntilPauseFinishes();

    let data;
    gActiveChild.sendManifest({
      contents: { kind: "debuggerRequest", request },
      onFinished(finishData) {
        data = finishData;
      },
    });
    gActiveChild.waitUntilPaused();

    if (data.restoredSnapshot) {
      // The child had an unhandled recording diverge and restored an earlier
      // checkpoint. Restore the child to the point it should be paused at and
      // fill its paused state back in by resending earlier debugger requests.
      pauseReplayingChild(gActiveChild, gPausePoint);
      return { unhandledDivergence: true };
    }

    if (data.divergedFromRecording) {
      // Remember whether the child diverged from the recording.
      gActiveChild.divergedFromRecording = true;
    }

    addDebuggerRequest(request);
    return data.response;
  },

  // Synchronously send a debugger request to the main child, which will always
  // be at the end of the recording and can receive requests even when the
  // active child is not currently paused.
  sendRequestMainChild(request) {
    gMainChild.waitUntilPaused(true);
    let data;
    gMainChild.sendManifest({
      contents: { kind: "debuggerRequest", request },
      onFinished(finishData) {
        data = finishData;
      },
    });
    gMainChild.waitUntilPaused();
    assert(!data.restoredSnapshot && !data.divergedFromRecording);
    return data.response;
  },

  resume,
  timeWarp,

  // Add a new logpoint.
  addLogpoint(logpoint) {
    gLogpoints.push(logpoint);
    forAllSavedCheckpoints(checkpoint =>
      findLogpointHits(checkpoint, logpoint)
    );
  },

  setActiveEventBreakpoints,

  unscannedRegions,
  cachedPoints,

  debuggerRequests() {
    return gDebuggerRequests;
  },

  getPauseDataAndRepaint() {
    // If the child has not arrived at the pause point yet, see if there is
    // cached pause data for this point already which we can immediately return.
    if (gPauseMode == PauseModes.ARRIVING && !gDebuggerRequests.length) {
      const data = maybeGetPauseData(gPausePoint);
      if (data) {
        // After the child pauses, it will need to generate the pause data so
        // that any referenced objects will be instantiated.
        addDebuggerRequest({ type: "pauseData" });
        RecordReplayControl.hadRepaint(data.paintData);
        return data;
      }
    }
    gControl.maybeSwitchToReplayingChild();
    const data = gControl.sendRequest({ type: "pauseData" });
    if (data.unhandledDivergence) {
      RecordReplayControl.clearGraphics();
    } else {
      addPauseData(gPausePoint, data, /* trackCached */ true);
      if (data.paintData) {
        RecordReplayControl.hadRepaint(data.paintData);
      }
    }
    return data;
  },

  paint(point) {
    const data = maybeGetPauseData(point);
    if (data && data.paintData) {
      RecordReplayControl.hadRepaint(data.paintData);
    }
  },

  isPausedAtDebuggerStatement() {
    const point = gControl.pausePoint();
    if (point) {
      const checkpoint = getSavedCheckpoint(point.checkpoint);
      const { debuggerStatements } = getCheckpointInfo(checkpoint);
      return pointArrayIncludes(debuggerStatements, point);
    }
    return false;
  },
};

///////////////////////////////////////////////////////////////////////////////
// Statistics
///////////////////////////////////////////////////////////////////////////////

let lastDumpTime = Date.now();

function maybeDumpStatistics() {
  const now = Date.now();
  if (now - lastDumpTime < 5000) {
    return;
  }
  lastDumpTime = now;

  let delayTotal = 0;
  let unscannedTotal = 0;
  let timeTotal = 0;
  let scanDurationTotal = 0;

  forAllSavedCheckpoints(checkpoint => {
    const checkpointTime = timeForSavedCheckpoint(checkpoint);
    const info = getCheckpointInfo(checkpoint);

    timeTotal += checkpointTime;
    if (info.scanTime) {
      delayTotal += checkpointTime * (info.scanTime - info.assignTime);
      scanDurationTotal += info.scanDuration;
    } else {
      unscannedTotal += checkpointTime;
    }
  });

  const memoryUsage = [];
  let totalSaved = 0;

  for (const child of gReplayingChildren) {
    if (!child) {
      continue;
    }
    totalSaved += child.savedCheckpoints.size;
    if (!child.lastMemoryUsage) {
      continue;
    }
    for (const [name, value] of Object.entries(child.lastMemoryUsage)) {
      if (!memoryUsage[name]) {
        memoryUsage[name] = 0;
      }
      memoryUsage[name] += value;
    }
  }

  const delay = delayTotal / timeTotal;
  const overhead = scanDurationTotal / (timeTotal - unscannedTotal);

  dumpv(`Statistics:`);
  dumpv(`Total recording time: ${timeTotal}`);
  dumpv(`Unscanned fraction: ${unscannedTotal / timeTotal}`);
  dumpv(`Average scan delay: ${delay}`);
  dumpv(`Average scanning overhead: ${overhead}`);

  dumpv(`Saved checkpoints: ${totalSaved}`);
  for (const [name, value] of Object.entries(memoryUsage)) {
    dumpv(`Memory ${name}: ${value}`);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Utilities
///////////////////////////////////////////////////////////////////////////////

function getPreference(name) {
  return Services.prefs.getBoolPref(`devtools.recordreplay.${name}`);
}

const loggingFullEnabled = getPreference("loggingFull");
const loggingEnabled = loggingFullEnabled || getPreference("logging");

// eslint-disable-next-line no-unused-vars
function ConnectDebugger(dbg) {
  gDebugger = dbg;
  dbg._control = gControl;
}

const startTime = Date.now();

// eslint-disable-next-line no-unused-vars
function currentTime() {
  return (((Date.now() - startTime) / 10) | 0) / 100;
}

function dumpv(str) {
  if (loggingEnabled) {
    dump(`[ReplayControl ${currentTime()}] ${str}\n`);
  }
}

function assert(v) {
  if (!v) {
    ThrowError("Assertion Failed!");
  }
}

function ThrowError(msg) {
  const error = new Error(msg);
  dump(`ReplayControl Server Error: ${msg} Stack: ${error.stack}\n`);
  throw error;
}

function stringify(object) {
  const str = JSON.stringify(object);
  if (str && str.length >= 4096 && !loggingFullEnabled) {
    return `${str.substr(0, 4096)} TRIMMED ${str.length}`;
  }
  return str;
}

// eslint-disable-next-line no-unused-vars
var EXPORTED_SYMBOLS = [
  "Initialize",
  "ConnectDebugger",
  "ManifestFinished",
  "BeforeSaveRecording",
  "AfterSaveRecording",
  "ChildCrashed",
];
