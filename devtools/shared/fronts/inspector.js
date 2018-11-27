/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Telemetry = require("devtools/client/shared/telemetry");
const telemetry = new Telemetry();
const TELEMETRY_EYEDROPPER_OPENED = "DEVTOOLS_EYEDROPPER_OPENED_COUNT";
const TELEMETRY_EYEDROPPER_OPENED_MENU = "DEVTOOLS_MENU_EYEDROPPER_OPENED_COUNT";
const SHOW_ALL_ANONYMOUS_CONTENT_PREF = "devtools.inspector.showAllAnonymousContent";
const SHOW_UA_SHADOW_ROOTS_PREF = "devtools.inspector.showUserAgentShadowRoots";

const {
  Front,
  FrontClassWithSpec,
  custom,
  preEvent,
  types,
} = require("devtools/shared/protocol.js");
const {
  inspectorSpec,
  walkerSpec,
} = require("devtools/shared/specs/inspector");

const Services = require("Services");
const defer = require("devtools/shared/defer");
loader.lazyRequireGetter(this, "nodeConstants",
  "devtools/shared/dom-node-constants");
loader.lazyRequireGetter(this, "Selection",
  "devtools/client/framework/selection", true);
loader.lazyRequireGetter(this, "flags",
  "devtools/shared/flags");

/**
 * Client side of the DOM walker.
 */
const WalkerFront = FrontClassWithSpec(walkerSpec, {
  // Set to true if cleanup should be requested after every mutation list.
  autoCleanup: true,

  /**
   * This is kept for backward-compatibility reasons with older remote target.
   * Targets previous to bug 916443
   */
  pick: custom(function() {
    return this._pick().then(response => {
      return response.node;
    });
  }, {impl: "_pick"}),

  initialize: function(client, form) {
    this._createRootNodePromise();
    Front.prototype.initialize.call(this, client, form);
    this._orphaned = new Set();
    this._retainedOrphans = new Set();
  },

  destroy: function() {
    Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function(json) {
    this.actorID = json.actor;
    this.rootNode = types.getType("domnode").read(json.root, this);
    this._rootNodeDeferred.resolve(this.rootNode);
    // FF42+ the actor starts exposing traits
    this.traits = json.traits || {};
  },

  /**
   * Clients can use walker.rootNode to get the current root node of the
   * walker, but during a reload the root node might be null.  This
   * method returns a promise that will resolve to the root node when it is
   * set.
   */
  getRootNode: function() {
    return this._rootNodeDeferred.promise;
  },

  /**
   * Create the root node promise, triggering the "new-root" notification
   * on resolution.
   */
  _createRootNodePromise: function() {
    this._rootNodeDeferred = defer();
    this._rootNodeDeferred.promise.then(() => {
      this.emit("new-root");
    });
  },

  /**
   * When reading an actor form off the wire, we want to hook it up to its
   * parent or host front.  The protocol guarantees that the parent will
   * be seen by the client in either a previous or the current request.
   * So if we've already seen this parent return it, otherwise create
   * a bare-bones stand-in node.  The stand-in node will be updated
   * with a real form by the end of the deserialization.
   */
  ensureDOMNodeFront: function(id) {
    const front = this.get(id);
    if (front) {
      return front;
    }

    return types.getType("domnode").read({ actor: id }, this, "standin");
  },

  /**
   * See the documentation for WalkerActor.prototype.retainNode for
   * information on retained nodes.
   *
   * From the client's perspective, `retainNode` can fail if the node in
   * question is removed from the ownership tree before the `retainNode`
   * request reaches the server.  This can only happen if the client has
   * asked the server to release nodes but hasn't gotten a response
   * yet: Either a `releaseNode` request or a `getMutations` with `cleanup`
   * set is outstanding.
   *
   * If either of those requests is outstanding AND releases the retained
   * node, this request will fail with noSuchActor, but the ownership tree
   * will stay in a consistent state.
   *
   * Because the protocol guarantees that requests will be processed and
   * responses received in the order they were sent, we get the right
   * semantics by setting our local retained flag on the node only AFTER
   * a SUCCESSFUL retainNode call.
   */
  retainNode: custom(function(node) {
    return this._retainNode(node).then(() => {
      node.retained = true;
    });
  }, {
    impl: "_retainNode",
  }),

  unretainNode: custom(function(node) {
    return this._unretainNode(node).then(() => {
      node.retained = false;
      if (this._retainedOrphans.has(node)) {
        this._retainedOrphans.delete(node);
        this._releaseFront(node);
      }
    });
  }, {
    impl: "_unretainNode",
  }),

  releaseNode: custom(function(node, options = {}) {
    // NodeFront.destroy will destroy children in the ownership tree too,
    // mimicking what the server will do here.
    const actorID = node.actorID;
    this._releaseFront(node, !!options.force);
    return this._releaseNode({ actorID: actorID });
  }, {
    impl: "_releaseNode",
  }),

  findInspectingNode: custom(function() {
    return this._findInspectingNode().then(response => {
      return response.node;
    });
  }, {
    impl: "_findInspectingNode",
  }),

  querySelector: custom(function(queryNode, selector) {
    return this._querySelector(queryNode, selector).then(response => {
      return response.node;
    });
  }, {
    impl: "_querySelector",
  }),

  gripToNodeFront: async function(grip) {
    const response = await this.getNodeActorFromObjectActor(grip.actor);
    const nodeFront = response ? response.node : null;
    if (!nodeFront) {
      throw new Error("The ValueGrip passed could not be translated to a NodeFront");
    }
    return nodeFront;
  },

  getNodeActorFromWindowID: custom(function(windowID) {
    return this._getNodeActorFromWindowID(windowID).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getNodeActorFromWindowID",
  }),

  getStyleSheetOwnerNode: custom(function(styleSheetActorID) {
    return this._getStyleSheetOwnerNode(styleSheetActorID).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getStyleSheetOwnerNode",
  }),

  getNodeFromActor: custom(function(actorID, path) {
    return this._getNodeFromActor(actorID, path).then(response => {
      return response ? response.node : null;
    });
  }, {
    impl: "_getNodeFromActor",
  }),

  /*
   * Incrementally search the document for a given string.
   * For modern servers, results will be searched with using the WalkerActor
   * `search` function (includes tag names, attributes, and text contents).
   * Only 1 result is sent back, and calling the method again with the same
   * query will send the next result. When there are no more results to be sent
   * back, null is sent.
   * @param {String} query
   * @param {Object} options
   *    - "reverse": search backwards
   */
  search: custom(async function(query, options = { }) {
    const searchData = this.searchData = this.searchData || { };
    const result = await this._search(query, options);
    const nodeList = result.list;

    // If this is a new search, start at the beginning.
    if (searchData.query !== query) {
      searchData.query = query;
      searchData.index = -1;
    }

    if (!nodeList.length) {
      return null;
    }

    // Move search result cursor and cycle if necessary.
    searchData.index = options.reverse ? searchData.index - 1 :
                                         searchData.index + 1;
    if (searchData.index >= nodeList.length) {
      searchData.index = 0;
    }
    if (searchData.index < 0) {
      searchData.index = nodeList.length - 1;
    }

    // Send back the single node, along with any relevant search data
    const node = await nodeList.item(searchData.index);
    return {
      type: "search",
      node: node,
      resultsLength: nodeList.length,
      resultsIndex: searchData.index,
    };
  }, {
    impl: "_search",
  }),

  _releaseFront: function(node, force) {
    if (node.retained && !force) {
      node.reparent(null);
      this._retainedOrphans.add(node);
      return;
    }

    if (node.retained) {
      // Forcing a removal.
      this._retainedOrphans.delete(node);
    }

    // Release any children
    for (const child of node.treeChildren()) {
      this._releaseFront(child, force);
    }

    // All children will have been removed from the node by this point.
    node.reparent(null);
    node.destroy();
  },

  /**
   * Get any unprocessed mutation records and process them.
   */
  getMutations: custom(function(options = {}) {
    return this._getMutations(options).then(mutations => {
      const emitMutations = [];
      for (const change of mutations) {
        // The target is only an actorID, get the associated front.
        let targetID;
        let targetFront;

        if (change.type === "newRoot") {
          // We may receive a new root without receiving any documentUnload
          // beforehand. Like when opening tools in middle of a document load.
          if (this.rootNode) {
            this._createRootNodePromise();
          }
          this.rootNode = types.getType("domnode").read(change.target, this);
          this._rootNodeDeferred.resolve(this.rootNode);
          targetID = this.rootNode.actorID;
          targetFront = this.rootNode;
        } else {
          targetID = change.target;
          targetFront = this.get(targetID);
        }

        if (!targetFront) {
          console.warn("Got a mutation for an unexpected actor: " + targetID +
            ", please file a bug on bugzilla.mozilla.org!");
          console.trace();
          continue;
        }

        const emittedMutation = Object.assign(change, { target: targetFront });

        if (change.type === "childList" ||
            change.type === "nativeAnonymousChildList") {
          // Update the ownership tree according to the mutation record.
          const addedFronts = [];
          const removedFronts = [];
          for (const removed of change.removed) {
            const removedFront = this.get(removed);
            if (!removedFront) {
              console.error("Got a removal of an actor we didn't know about: " +
                removed);
              continue;
            }
            // Remove from the ownership tree
            removedFront.reparent(null);

            // This node is orphaned unless we get it in the 'added' list
            // eventually.
            this._orphaned.add(removedFront);
            removedFronts.push(removedFront);
          }
          for (const added of change.added) {
            const addedFront = this.get(added);
            if (!addedFront) {
              console.error("Got an addition of an actor we didn't know " +
                "about: " + added);
              continue;
            }
            addedFront.reparent(targetFront);

            // The actor is reconnected to the ownership tree, unorphan
            // it.
            this._orphaned.delete(addedFront);
            addedFronts.push(addedFront);
          }

          // Before passing to users, replace the added and removed actor
          // ids with front in the mutation record.
          emittedMutation.added = addedFronts;
          emittedMutation.removed = removedFronts;

          // If this is coming from a DOM mutation, the actor's numChildren
          // was passed in. Otherwise, it is simulated from a frame load or
          // unload, so don't change the front's form.
          if ("numChildren" in change) {
            targetFront._form.numChildren = change.numChildren;
          }
        } else if (change.type === "frameLoad") {
          // Nothing we need to do here, except verify that we don't have any
          // document children, because we should have gotten a documentUnload
          // first.
          for (const child of targetFront.treeChildren()) {
            if (child.nodeType === nodeConstants.DOCUMENT_NODE) {
              console.warn("Got an unexpected frameLoad in the inspector, " +
                "please file a bug on bugzilla.mozilla.org!");
              console.trace();
            }
          }
        } else if (change.type === "documentUnload") {
          if (targetFront === this.rootNode) {
            this._createRootNodePromise();
          }

          // We try to give fronts instead of actorIDs, but these fronts need
          // to be destroyed now.
          emittedMutation.target = targetFront.actorID;
          emittedMutation.targetParent = targetFront.parentNode();

          // Release the document node and all of its children, even retained.
          this._releaseFront(targetFront, true);
        } else if (change.type === "shadowRootAttached") {
          targetFront._form.isShadowHost = true;
        } else if (change.type === "customElementDefined") {
          targetFront._form.customElementLocation = change.customElementLocation;
        } else if (change.type === "unretained") {
          // Retained orphans were force-released without the intervention of
          // client (probably a navigated frame).
          for (const released of change.nodes) {
            const releasedFront = this.get(released);
            this._retainedOrphans.delete(released);
            this._releaseFront(releasedFront, true);
          }
        } else {
          targetFront.updateMutation(change);
        }

        // Update the inlineTextChild property of the target for a selected list of
        // mutation types.
        if (change.type === "inlineTextChild" ||
            change.type === "childList" ||
            change.type === "shadowRootAttached" ||
            change.type === "nativeAnonymousChildList") {
          if (change.inlineTextChild) {
            targetFront.inlineTextChild =
              types.getType("domnode").read(change.inlineTextChild, this);
          } else {
            targetFront.inlineTextChild = undefined;
          }
        }

        emitMutations.push(emittedMutation);
      }

      if (options.cleanup) {
        for (const node of this._orphaned) {
          // This will move retained nodes to this._retainedOrphans.
          this._releaseFront(node);
        }
        this._orphaned = new Set();
      }

      this.emit("mutations", emitMutations);
    });
  }, {
    impl: "_getMutations",
  }),

  /**
   * Handle the `new-mutations` notification by fetching the
   * available mutation records.
   */
  onMutations: preEvent("new-mutations", function() {
    // Fetch and process the mutations.
    this.getMutations({cleanup: this.autoCleanup}).catch(() => {});
  }),

  isLocal: function() {
    return !!this.conn._transport._serverConnection;
  },

  removeNode: custom(async function(node) {
    const previousSibling = await this.previousSibling(node);
    const nextSibling = await this._removeNode(node);
    return {
      previousSibling: previousSibling,
      nextSibling: nextSibling,
    };
  }, {
    impl: "_removeNode",
  }),
});

exports.WalkerFront = WalkerFront;

/**
 * Client side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorFront = FrontClassWithSpec(inspectorSpec, {
  initialize: async function(client, tabForm) {
    Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.inspectorActor;
    this._client = client;
    this._highlighters = new Map();

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    this.manage(this);

    // async initialization
    await Promise.all([
      this._getWalker(),
      this._getHighlighter(),
    ]);

    this.selection = new Selection(this.walker);
  },

  _getWalker: async function() {
    const showAllAnonymousContent = Services.prefs.getBoolPref(
      SHOW_ALL_ANONYMOUS_CONTENT_PREF);
    const showUserAgentShadowRoots = Services.prefs.getBoolPref(
      SHOW_UA_SHADOW_ROOTS_PREF);
    this.walker = await this.getWalker({
      showAllAnonymousContent,
      showUserAgentShadowRoots,
    });
  },

  _getHighlighter: async function() {
    const autohide = !flags.testing;
    this.highlighter = await this.getHighlighter(autohide);
  },

  hasHighlighter(type) {
    return this._highlighters.has(type);
  },

  destroy: function() {
    // Selection isn't a Front and so isn't managed by InspectorFront
    // and has to be destroyed manually
    this.selection.destroy();
    // Highlighter fronts are managed by InspectorFront and so will be
    // automatically destroyed. But we have to clear the `_highlighters`
    // Map as well as explicitly call `finalize` request on all of them.
    this.destroyHighlighters();
    Front.prototype.destroy.call(this);
  },

  destroyHighlighters: function() {
    for (const type of this._highlighters.keys()) {
      if (this._highlighters.has(type)) {
        this._highlighters.get(type).finalize();
        this._highlighters.delete(type);
      }
    }
  },

  getKnownHighlighter: function(type) {
    return this._highlighters.get(type);
  },

  getOrCreateHighlighterByType: async function(type) {
    let front =  this._highlighters.get(type);
    if (!front) {
      front = await this.getHighlighterByType(type);
      this._highlighters.set(type, front);
    }
    return front;
  },

  pickColorFromPage: custom(async function(options) {
    await this._pickColorFromPage(options);
    if (options && options.fromMenu) {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED_MENU).add(true);
    } else {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED).add(true);
    }
  }, {
    impl: "_pickColorFromPage",
  }),
});

exports.InspectorFront = InspectorFront;
