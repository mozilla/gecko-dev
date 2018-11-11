/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Import as different name `coreEmit`, so we don't conflict
// with the global `window` listener itself.
const { emit: coreEmit } = require("sdk/event/core");

/**
 * Representational wrapper around AudioNodeActors. Adding and destroying
 * AudioNodes should be performed through the AudioNodes collection.
 *
 * Events:
 * - `connect`: node, destinationNode, parameter
 * - `disconnect`: node
 */
const AudioNodeModel = Class({
  extends: EventTarget,

  // Will be added via AudioNodes `add`
  collection: null,

  initialize: function (actor) {
    this.actor = actor;
    this.id = actor.actorID;
    this.type = actor.type;
    this.bypassable = actor.bypassable;
    this._bypassed = false;
    this.connections = [];
  },

  /**
   * Stores connection data inside this instance of this audio node connecting
   * to another node (destination). If connecting to another node's AudioParam,
   * the second argument (param) must be populated with a string.
   *
   * Connecting nodes is idempotent. Upon new connection, emits "connect" event.
   *
   * @param AudioNodeModel destination
   * @param String param
   */
  connect: function (destination, param) {
    let edge = findWhere(this.connections, { destination: destination.id, param: param });

    if (!edge) {
      this.connections.push({ source: this.id, destination: destination.id, param: param });
      coreEmit(this, "connect", this, destination, param);
    }
  },

  /**
   * Clears out all internal connection data. Emits "disconnect" event.
   */
  disconnect: function () {
    this.connections.length = 0;
    coreEmit(this, "disconnect", this);
  },

  /**
   * Gets the bypass status of the audio node.
   *
   * @return Boolean
   */
  isBypassed: function () {
    return this._bypassed;
  },

  /**
   * Sets the bypass value of an AudioNode.
   *
   * @param Boolean enable
   * @return Promise
   */
  bypass: function (enable) {
    this._bypassed = enable;
    return this.actor.bypass(enable).then(() => coreEmit(this, "bypass", this, enable));
  },

  /**
   * Returns a promise that resolves to an array of objects containing
   * both a `param` name property and a `value` property.
   *
   * @return Promise->Object
   */
  getParams: function () {
    return this.actor.getParams();
  },

  /**
   * Returns a promise that resolves to an object containing an
   * array of event information and an array of automation data.
   *
   * @param String paramName
   * @return Promise->Array
   */
  getAutomationData: function (paramName) {
    return this.actor.getAutomationData(paramName);
  },

  /**
   * Takes a `dagreD3.Digraph` object and adds this node to
   * the graph to be rendered.
   *
   * @param dagreD3.Digraph
   */
  addToGraph: function (graph) {
    graph.addNode(this.id, {
      type: this.type,
      label: this.type.replace(/Node$/, ""),
      id: this.id,
      bypassed: this._bypassed
    });
  },

  /**
   * Takes a `dagreD3.Digraph` object and adds edges to
   * the graph to be rendered. Separate from `addToGraph`,
   * as while we depend on D3/Dagre's constraints, we cannot
   * add edges for nodes that have not yet been added to the graph.
   *
   * @param dagreD3.Digraph
   */
  addEdgesToGraph: function (graph) {
    for (let edge of this.connections) {
      let options = {
        source: this.id,
        target: edge.destination
      };

      // Only add `label` if `param` specified, as this is an AudioParam
      // connection then. `label` adds the magic to render with dagre-d3,
      // and `param` is just more explicitly the param, ignoring
      // implementation details.
      if (edge.param) {
        options.label = options.param = edge.param;
      }

      graph.addEdge(null, this.id, edge.destination, options);
    }
  },

  toString: () => "[object AudioNodeModel]",
});


/**
 * Constructor for a Collection of `AudioNodeModel` models.
 *
 * Events:
 * - `add`: node
 * - `remove`: node
 * - `connect`: node, destinationNode, parameter
 * - `disconnect`: node
 */
const AudioNodesCollection = Class({
  extends: EventTarget,

  model: AudioNodeModel,

  initialize: function () {
    this.models = new Set();
    this._onModelEvent = this._onModelEvent.bind(this);
  },

  /**
   * Iterates over all models within the collection, calling `fn` with the
   * model as the first argument.
   *
   * @param Function fn
   */
  forEach: function (fn) {
    this.models.forEach(fn);
  },

  /**
   * Creates a new AudioNodeModel, passing through arguments into the AudioNodeModel
   * constructor, and adds the model to the internal collection store of this
   * instance.
   *
   * Emits "add" event on instance when completed.
   *
   * @param Object obj
   * @return AudioNodeModel
   */
  add: function (obj) {
    let node = new this.model(obj);
    node.collection = this;

    this.models.add(node);

    node.on("*", this._onModelEvent);
    coreEmit(this, "add", node);
    return node;
  },

  /**
   * Removes an AudioNodeModel from the internal collection. Calls `delete` method
   * on the model, and emits "remove" on this instance.
   *
   * @param AudioNodeModel node
   */
  remove: function (node) {
    this.models.delete(node);
    coreEmit(this, "remove", node);
  },

  /**
   * Empties out the internal collection of all AudioNodeModels.
   */
  reset: function () {
    this.models.clear();
  },

  /**
   * Takes an `id` from an AudioNodeModel and returns the corresponding
   * AudioNodeModel within the collection that matches that id. Returns `null`
   * if not found.
   *
   * @param Number id
   * @return AudioNodeModel|null
   */
  get: function (id) {
    return findWhere(this.models, { id: id });
  },

  /**
   * Returns the count for how many models are a part of this collection.
   *
   * @return Number
   */
  get length() {
    return this.models.size;
  },

  /**
   * Returns detailed information about the collection. used during tests
   * to query state. Returns an object with information on node count,
   * how many edges are within the data graph, as well as how many of those edges
   * are for AudioParams.
   *
   * @return Object
   */
  getInfo: function () {
    let info = {
      nodes: this.length,
      edges: 0,
      paramEdges: 0
    };

    this.models.forEach(node => {
      let paramEdgeCount = node.connections.filter(edge => edge.param).length;
      info.edges += node.connections.length - paramEdgeCount;
      info.paramEdges += paramEdgeCount;
    });
    return info;
  },

  /**
   * Adds all nodes within the collection to the passed in graph,
   * as well as their corresponding edges.
   *
   * @param dagreD3.Digraph
   */
  populateGraph: function (graph) {
    this.models.forEach(node => node.addToGraph(graph));
    this.models.forEach(node => node.addEdgesToGraph(graph));
  },

  /**
   * Called when a stored model emits any event. Used to manage
   * event propagation, or listening to model events to react, like
   * removing a model from the collection when it's destroyed.
   */
  _onModelEvent: function (eventName, node, ...args) {
    if (eventName === "remove") {
      // If a `remove` event from the model, remove it
      // from the collection, and let the method handle the emitting on
      // the collection
      this.remove(node);
    } else {
      // Pipe the event to the collection
      coreEmit(this, eventName, node, ...args);
    }
  },

  toString: () => "[object AudioNodeCollection]",
});
