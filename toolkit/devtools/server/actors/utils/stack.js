/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let {Class} = require("sdk/core/heritage");

/**
 * A helper class that stores stack frame objects.  Each frame is
 * assigned an index, and if a frame is added more than once, the same
 * index is used.  Users of the class can get an array of all frames
 * that have been added.
 */
let StackFrameCache = Class({
  /**
   * Initialize this object.
   */
  initialize: function() {
    this._framesToCounts = null;
    this._framesToIndices = null;
    this._framesToForms = null;
    this._lastEventSize = 0;
  },

  /**
   * Prepare to accept frames.
   */
  initFrames: function() {
    if (this._framesToCounts) {
      // The maps are already initialized.
      return;
    }

    this._framesToCounts = new Map();
    this._framesToIndices = new Map();
    this._framesToForms = new Map();
    this._lastEventSize = 0;
  },

  /**
   * Forget all stored frames and reset to the initialized state.
   */
  clearFrames: function() {
    this._framesToCounts.clear();
    this._framesToCounts = null;
    this._framesToIndices.clear();
    this._framesToIndices = null;
    this._framesToForms.clear();
    this._framesToForms = null;
    this._lastEventSize = 0;
  },

  /**
   * Add a frame to this stack frame cache, and return the index of
   * the frame.
   */
  addFrame: function(frame) {
    this._assignFrameIndices(frame);
    this._createFrameForms(frame);
    this._countFrame(frame);
    return this._framesToIndices.get(frame);
  },

  /**
   * A helper method for the memory actor.  This populates the packet
   * object with "frames" and "counts" properties.  Each of these
   * properties will be an array indexed by frame ID.  "frames" will
   * contain frame objects (see makeEvent) and "counts" will hold
   * allocation counts for each frame.
   *
   * @param packet
   *        The packet to update.
   *
   * @returns packet
   */
  updateFramePacket: function(packet) {
    // Now that we are guaranteed to have a form for every frame, we know the
    // size the "frames" property's array must be. We use that information to
    // create dense arrays even though we populate them out of order.
    const size = this._framesToForms.size;
    packet.frames = Array(size).fill(null);
    packet.counts = Array(size).fill(0);

    // Populate the "frames" and "counts" properties.
    for (let [stack, index] of this._framesToIndices) {
      packet.frames[index] = this._framesToForms.get(stack);
      packet.counts[index] = this._framesToCounts.get(stack) || 0;
    }

    return packet;
  },

  /**
   * If any new stack frames have been added to this cache since the
   * last call to makeEvent (clearing the cache also resets the "last
   * call"), then return a new array describing the new frames.  If no
   * new frames are available, return null.
   *
   * The frame cache assumes that the user of the cache keeps track of
   * all previously-returned arrays and, in theory, concatenates them
   * all to form a single array holding all frames added to the cache
   * since the last reset.  This concatenated array can be indexed by
   * the frame ID.  The array returned by this function, though, is
   * dense and starts at 0.
   *
   * Each element in the array is an object of the form:
   * {
   *   line: <line number for this frame>,
   *   column: <column number for this frame>,
   *   source: <filename string for this frame>,
   *   functionDisplayName: <this frame's inferred function name function or null>,
   *   parent: <frame ID -- an index into the concatenated array mentioned above>
   *   asyncCause: the async cause, or null
   *   asyncParent: <frame ID -- an index into the concatenated array mentioned above>
   * }
   *
   * The intent of this approach is to make it simpler to efficiently
   * send frame information over the debugging protocol, by only
   * sending new frames.
   *
   * @returns array or null
   */
  makeEvent: function() {
    const size = this._framesToForms.size;
    if (!size || size <= this._lastEventSize) {
      return null;
    }

    let packet = Array(size - this._lastEventSize).fill(null);
    for (let [stack, index] of this._framesToIndices) {
      if (index >= this._lastEventSize) {
        packet[index - this._lastEventSize] = this._framesToForms.get(stack);
      }
    }

    this._lastEventSize = size;

    return packet;
  },

  /**
   * Assigns an index to the given frame and its parents, if an index is not
   * already assigned.
   *
   * @param SavedFrame frame
   *        A frame to assign an index to.
   */
  _assignFrameIndices: function(frame) {
    if (this._framesToIndices.has(frame)) {
      return;
    }

    if (frame) {
      this._assignFrameIndices(frame.parent);
      this._assignFrameIndices(frame.asyncParent);
    }

    const index = this._framesToIndices.size;
    this._framesToIndices.set(frame, index);
  },

  /**
   * Create the form for the given frame, if one doesn't already exist.
   *
   * @param SavedFrame frame
   *        A frame to create a form for.
   */
  _createFrameForms: function(frame) {
    if (this._framesToForms.has(frame)) {
      return;
    }

    let form = null;
    if (frame) {
      form = {
        line: frame.line,
        column: frame.column,
        source: frame.source,
        functionDisplayName: frame.functionDisplayName,
        parent: this._framesToIndices.get(frame.parent),
        asyncParent: this._framesToIndices.get(frame.asyncParent),
        asyncCause: frame.asyncCause
      };
      this._createFrameForms(frame.parent);
      this._createFrameForms(frame.asyncParent);
    }

    this._framesToForms.set(frame, form);
  },

  /**
   * Increment the allocation count for the provided frame.
   *
   * @param SavedFrame frame
   *        The frame whose allocation count should be incremented.
   */
  _countFrame: function(frame) {
    if (!this._framesToCounts.has(frame)) {
      this._framesToCounts.set(frame, 1);
    } else {
      let count = this._framesToCounts.get(frame);
      this._framesToCounts.set(frame, count + 1);
    }
  }
});

exports.StackFrameCache = StackFrameCache;
