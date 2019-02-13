/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");

loader.lazyRequireGetter(this, "L10N",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "CATEGORY_MAPPINGS",
  "devtools/performance/global", true);
loader.lazyRequireGetter(this, "JITOptimizations",
  "devtools/performance/jit", true);
loader.lazyRequireGetter(this, "FrameUtils",
  "devtools/performance/frame-utils");

/**
 * A call tree for a thread. This is essentially a linkage between all frames
 * of all samples into a single tree structure, with additional information
 * on each node, like the time spent (in milliseconds) and samples count.
 *
 * @param object thread
 *        The raw thread object received from the backend. Contains samples,
 *        stackTable, frameTable, and stringTable.
 * @param object options
 *        Additional supported options
 *          - number startTime
 *          - number endTime
 *          - boolean contentOnly [optional]
 *          - boolean invertTree [optional]
 *          - boolean flattenRecursion [optional]
 */
function ThreadNode(thread, options = {}) {
  if (options.endTime == void 0 || options.startTime == void 0) {
    throw new Error("ThreadNode requires both `startTime` and `endTime`.");
  }
  this.samples = 0;
  this.sampleTimes = [];
  this.youngestFrameSamples = 0;
  this.calls = [];
  this.duration = options.endTime - options.startTime;

  let { samples, stackTable, frameTable, stringTable, allocationsTable } = thread;

  // Nothing to do if there are no samples.
  if (samples.data.length === 0) {
    return;
  }

  this._buildInverted(samples, stackTable, frameTable, stringTable, allocationsTable, options);
  if (!options.invertTree) {
    this._uninvert();
  }
}

ThreadNode.prototype = {
  /**
   * Build an inverted call tree from profile samples. The format of the
   * samples is described in tools/profiler/ProfileEntry.h, under the heading
   * "ThreadProfile JSON Format".
   *
   * The profile data is naturally presented inverted. Inverting the call tree
   * is also the default in the Performance tool.
   *
   * @param object samples
   *        The raw samples array received from the backend.
   * @param object stackTable
   *        The table of deduplicated stacks from the backend.
   * @param object frameTable
   *        The table of deduplicated frames from the backend.
   * @param object stringTable
   *        The table of deduplicated strings from the backend.
   * @param object allocationsTable
   *        The table of allocation counts from the backend. Indexed by frame
   *        index.
   * @param object options
   *        Additional supported options
   *          - number startTime
   *          - number endTime
   *          - boolean contentOnly [optional]
   *          - boolean invertTree [optional]
   */
  _buildInverted: function buildInverted(samples, stackTable, frameTable, stringTable, allocationsTable, options) {
    function getOrAddFrameNode(calls, isLeaf, frameKey, inflatedFrame, isMetaCategory, leafTable) {
      // Insert the inflated frame into the call tree at the current level.
      let frameNode;

      // Leaf nodes have fan out much greater than non-leaf nodes, thus the
      // use of a hash table. Otherwise, do linear search.
      //
      // Note that this method is very hot, thus the manual looping over
      // Array.prototype.find.
      if (isLeaf) {
        frameNode = leafTable[frameKey];
      } else {
        for (let i = 0; i < calls.length; i++) {
          if (calls[i].key === frameKey) {
            frameNode = calls[i];
            break;
          }
        }
      }

      if (!frameNode) {
        frameNode = new FrameNode(frameKey, inflatedFrame, isMetaCategory);
        if (isLeaf) {
          leafTable[frameKey] = frameNode;
        }
        calls.push(frameNode);
      }

      return frameNode;
    }

    const SAMPLE_STACK_SLOT = samples.schema.stack;
    const SAMPLE_TIME_SLOT = samples.schema.time;

    const STACK_PREFIX_SLOT = stackTable.schema.prefix;
    const STACK_FRAME_SLOT = stackTable.schema.frame;

    const InflatedFrame = FrameUtils.InflatedFrame;
    const getOrAddInflatedFrame = FrameUtils.getOrAddInflatedFrame;

    let samplesData = samples.data;
    let stacksData = stackTable.data;

    // Caches.
    let inflatedFrameCache = FrameUtils.getInflatedFrameCache(frameTable);
    let leafTable = Object.create(null);

    let startTime = options.startTime;
    let endTime = options.endTime;
    let flattenRecursion = options.flattenRecursion;

    // Reused options object passed to InflatedFrame.prototype.getFrameKey.
    let mutableFrameKeyOptions = {
      contentOnly: options.contentOnly,
      isRoot: false,
      isLeaf: false,
      isMetaCategoryOut: false
    };

    for (let i = 0; i < samplesData.length; i++) {
      let sample = samplesData[i];
      let sampleTime = sample[SAMPLE_TIME_SLOT];

      // A sample's end time is considered to be its time of sampling. Its
      // start time is the sampling time of the previous sample.
      //
      // Thus, we compare sampleTime <= start instead of < to filter out
      // samples that end exactly at the start time.
      if (!sampleTime || sampleTime <= startTime || sampleTime > endTime) {
        continue;
      }

      let stackIndex = sample[SAMPLE_STACK_SLOT];
      let calls = this.calls;
      let prevCalls = this.calls;
      let prevFrameKey;
      let isLeaf = mutableFrameKeyOptions.isLeaf = true;
      let skipRoot = options.invertTree;

      // Inflate the stack and build the FrameNode call tree directly.
      //
      // In the profiler data, each frame's stack is referenced by an index
      // into stackTable.
      //
      // Each entry in stackTable is a pair [ prefixIndex, frameIndex ]. The
      // prefixIndex is itself an index into stackTable, referencing the
      // prefix of the current stack (that is, the younger frames). In other
      // words, the stackTable is encoded as a trie of the inverted
      // callstack. The frameIndex is an index into frameTable, describing the
      // frame at the current depth.
      //
      // This algorithm inflates each frame in the frame table while walking
      // the stack trie as described above.
      //
      // The frame key is then computed from the inflated frame /and/ the
      // current depth in the FrameNode call tree.  That is, the frame key is
      // not wholly determinable from just the inflated frame.
      //
      // For content frames, the frame key is just its location. For chrome
      // frames, the key may be a metacategory or its location, depending on
      // rendering options and its position in the FrameNode call tree.
      //
      // The frame key is then used to build up the inverted FrameNode call
      // tree.
      //
      // Note that various filtering functions, such as filtering for content
      // frames or flattening recursion, are inlined into the stack inflation
      // loop. This is important for performance as it avoids intermediate
      // structures and multiple passes.
      while (stackIndex !== null) {
        let stackEntry = stacksData[stackIndex];
        let frameIndex = stackEntry[STACK_FRAME_SLOT];

        // Fetch the stack prefix (i.e. older frames) index.
        stackIndex = stackEntry[STACK_PREFIX_SLOT];

        // Do not include the (root) node in this sample, as the costs of each frame
        // will make it clear to differentiate (root)->B vs (root)->A->B
        // when a tree is inverted, a revert of bug 1147604
        if (stackIndex === null && skipRoot) {
          break;
        }

        // Inflate the frame.
        let inflatedFrame = getOrAddInflatedFrame(inflatedFrameCache, frameIndex, frameTable,
                                                  stringTable, allocationsTable);

        // Compute the frame key.
        mutableFrameKeyOptions.isRoot = stackIndex === null;
        let frameKey = inflatedFrame.getFrameKey(mutableFrameKeyOptions);

        // An empty frame key means this frame should be skipped.
        if (frameKey === "") {
          continue;
        }

        // If we shouldn't flatten the current frame into the previous one, advance a
        // level in the call tree.
        if (!flattenRecursion || frameKey !== prevFrameKey) {
          calls = prevCalls;
        }

        let frameNode = getOrAddFrameNode(calls, isLeaf, frameKey, inflatedFrame,
                                          mutableFrameKeyOptions.isMetaCategoryOut,
                                          leafTable);
        if (isLeaf) {
          frameNode.youngestFrameSamples++;
          if (inflatedFrame.optimizations) {
            frameNode._addOptimizations(inflatedFrame.optimizations, inflatedFrame.implementation,
                                        sampleTime, stringTable);
          }
        }
        frameNode.samples++;

        prevFrameKey = frameKey;
        prevCalls = frameNode.calls;
        isLeaf = mutableFrameKeyOptions.isLeaf = false;
      }

      this.samples++;
      this.sampleTimes.push(sampleTime);
    }
  },

  /**
   * Uninverts the call tree after its having been built.
   */
  _uninvert: function uninvert() {
    function mergeOrAddFrameNode(calls, node) {
      // Unlike the inverted call tree, we don't use a root table for the top
      // level, as in general, there are many fewer entry points than
      // leaves. Instead, linear search is used regardless of level.
      for (let i = 0; i < calls.length; i++) {
        if (calls[i].key === node.key) {
          let foundNode = calls[i];
          foundNode._merge(node);
          return foundNode.calls;
        }
      }
      let copy = node._clone();
      calls.push(copy);
      return copy.calls;
    }

    let workstack = [{ node: this, level: 0 }];
    let spine = [];
    let entry;

    // The new root.
    let rootCalls = [];

    // Walk depth-first and keep the current spine (e.g., callstack).
    while (entry = workstack.pop()) {
      spine[entry.level] = entry;

      let node = entry.node;
      let calls = node.calls;

      if (calls.length === 0) {
        // We've bottomed out. Reverse the spine and add them to the
        // uninverted call tree.
        let uninvertedCalls = rootCalls;
        for (let level = entry.level; level > 0; level--) {
          let callee = spine[level];
          uninvertedCalls = mergeOrAddFrameNode(uninvertedCalls, callee.node);
        }
      } else {
        // We still have children. Continue the depth-first walk.
        for (let i = 0; i < calls.length; i++) {
          workstack.push({ node: calls[i], level: entry.level + 1 });
        }
      }
    }

    // Replace the toplevel calls with rootCalls, which now contains the
    // uninverted roots.
    this.calls = rootCalls;
  },

  /**
   * Gets additional details about this node.
   * @return object
   */
  getInfo: function() {
    return {
      nodeType: "Thread",
      functionName: L10N.getStr("table.root"),
      categoryData: {}
    };
  },

  /**
   * Mimicks the interface of FrameNode, and a ThreadNode can never have
   * optimization data (at the moment, anyway), so provide a function
   * to return null so we don't need to check if a frame node is a thread
   * or not everytime we fetch optimization data.
   *
   * @return {null}
   */

  hasOptimizations: function () {
    return null;
  }
};

/**
 * A function call node in a tree. Represents a function call with a unique context,
 * resulting in each FrameNode having its own row in the corresponding tree view.
 * Take samples:
 *  A()->B()->C()
 *  A()->B()
 *  Q()->B()
 *
 * In inverted tree, A()->B()->C() would have one frame node, and A()->B() and
 * Q()->B() would share a frame node.
 * In an uninverted tree, A()->B()->C() and A()->B() would share a frame node,
 * with Q()->B() having its own.
 *
 * In all cases, all the frame nodes originated from the same InflatedFrame.
 *
 * @param string frameKey
 *        The key associated with this frame. The key determines identity of
 *        the node.
 * @param string location
 *        The location of this function call. Note that this isn't sanitized,
 *        so it may very well (not?) include the function name, url, etc.
 * @param number line
 *        The line number inside the source containing this function call.
 * @param number category
 *        The category type of this function call ("js", "graphics" etc.).
 * @param number allocations
 *        The number of memory allocations performed in this frame.
 * @param number isContent
 *        Whether this frame is content.
 * @param boolean isMetaCategory
 *        Whether or not this is a platform node that should appear as a
 *        generalized meta category or not.
 */
function FrameNode(frameKey, { location, line, category, allocations, isContent }, isMetaCategory) {
  this.key = frameKey;
  this.location = location;
  this.line = line;
  this.allocations = allocations;
  this.youngestFrameSamples = 0;
  this.samples = 0;
  this.calls = [];
  this.isContent = !!isContent;
  this._optimizations = null;
  this._tierData = null;
  this._stringTable = null;
  this.isMetaCategory = !!isMetaCategory;
  this.category = category;
}

FrameNode.prototype = {
  /**
   * Take optimization data observed for this frame.
   *
   * @param object optimizationSite
   *               Any JIT optimization information attached to the current
   *               sample. Lazily inflated via stringTable.
   * @param number implementation
   *               JIT implementation used for this observed frame (interpreter,
   *               baseline, ion);
   * @param number time
   *               The time this optimization occurred.
   * @param object stringTable
   *               The string table used to inflate the optimizationSite.
   */
  _addOptimizations: function (site, implementation, time, stringTable) {
    // Simply accumulate optimization sites for now. Processing is done lazily
    // by JITOptimizations, if optimization information is actually displayed.
    if (site) {
      let opts = this._optimizations;
      if (opts === null) {
        opts = this._optimizations = [];
        this._stringTable = stringTable;
      }
      opts.push(site);

      if (this._tierData === null) {
        this._tierData = [];
      }
      // Record type of implementation used and the sample time
      this._tierData.push({ implementation, time });
    }
  },

  _clone: function () {
    let newNode = new FrameNode(this.key, this, this.isMetaCategory);
    newNode._merge(this);
    return newNode;
  },

  _merge: function (otherNode) {
    if (this === otherNode) {
      return;
    }

    this.samples += otherNode.samples;
    this.youngestFrameSamples += otherNode.youngestFrameSamples;

    if (otherNode._optimizations) {
      let opts = this._optimizations;
      if (opts === null) {
        opts = this._optimizations = [];
        this._stringTable = otherNode._stringTable;
      }
      let otherOpts = otherNode._optimizations;
      for (let i = 0; i < otherOpts.length; i++) {
        opts.push(otherOpts[i]);
      }
    }
  },

  /**
   * Returns the parsed location and additional data describing
   * this frame. Uses cached data if possible.
   *
   * @return object
   *         The computed { name, file, url, line } properties for this
   *         function call.
   */
  getInfo: function() {
    return this._data || this._computeInfo();
  },

  /**
   * Parses the raw location of this function call to retrieve the actual
   * function name and source url.
   */
  _computeInfo: function() {
    let categoryData = CATEGORY_MAPPINGS[this.category] || {};
    let parsedData = FrameUtils.parseLocation(this.location, this.line, this.column);
    parsedData.nodeType = "Frame";
    parsedData.categoryData = categoryData;
    parsedData.isContent = this.isContent;
    parsedData.isMetaCategory = this.isMetaCategory;
    parsedData.hasOptimizations = this.hasOptimizations();

    return this._data = parsedData;
  },

  /**
   * Returns whether or not the frame node has an JITOptimizations model.
   *
   * @return {Boolean}
   */
  hasOptimizations: function () {
    return !this.isMetaCategory && !!this._optimizations;
  },

  /**
   * Returns the underlying JITOptimizations model representing
   * the optimization attempts occuring in this frame.
   *
   * @return {JITOptimizations|null}
   */
  getOptimizations: function () {
    if (!this._optimizations) {
      return null;
    }
    return new JITOptimizations(this._optimizations, this._stringTable);
  },

  /**
   * Returns the optimization tiers used overtime.
   *
   * @return {?Array<object>}
   */
  getOptimizationTierData: function () {
    if (!this._tierData) {
      return null;
    }
    return this._tierData;
  }
};

exports.ThreadNode = ThreadNode;
exports.FrameNode = FrameNode;
