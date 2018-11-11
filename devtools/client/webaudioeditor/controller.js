/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {PrefObserver} = require("devtools/client/shared/prefs");

/**
 * A collection of `AudioNodeModel`s used throughout the editor
 * to keep track of audio nodes within the audio context.
 */
var gAudioNodes = new AudioNodesCollection();

/**
 * Initializes the web audio editor views
 */
function startupWebAudioEditor() {
  return all([
    WebAudioEditorController.initialize(),
    ContextView.initialize(),
    InspectorView.initialize(),
    PropertiesView.initialize(),
    AutomationView.initialize(),
  ]);
}

/**
 * Destroys the web audio editor controller and views.
 */
function shutdownWebAudioEditor() {
  return all([
    WebAudioEditorController.destroy(),
    ContextView.destroy(),
    InspectorView.destroy(),
    PropertiesView.destroy(),
    AutomationView.destroy(),
  ]);
}

/**
 * Functions handling target-related lifetime events.
 */
var WebAudioEditorController = {
  /**
   * Listen for events emitted by the current tab target.
   */
  async initialize() {
    this._onTabWillNavigate = this._onTabWillNavigate.bind(this);
    this._onThemeChange = this._onThemeChange.bind(this);

    gTarget.on("will-navigate", this._onTabWillNavigate);
    gFront.on("start-context", this._onStartContext);
    gFront.on("create-node", this._onCreateNode);
    gFront.on("connect-node", this._onConnectNode);
    gFront.on("connect-param", this._onConnectParam);
    gFront.on("disconnect-node", this._onDisconnectNode);
    gFront.on("change-param", this._onChangeParam);
    gFront.on("destroy-node", this._onDestroyNode);

    // Hook into theme change so we can change
    // the graph's marker styling, since we can't do this
    // with CSS

    this._prefObserver = new PrefObserver("");
    this._prefObserver.on("devtools.theme", this._onThemeChange);

    // Store the AudioNode definitions from the WebAudioFront, if the method exists.
    // If not, get the JSON directly. Using the actor method is preferable so the client
    // knows exactly what methods are supported on the server.
    const actorHasDefinition = await gTarget.actorHasMethod("webaudio", "getDefinition");
    if (actorHasDefinition) {
      AUDIO_NODE_DEFINITION = await gFront.getDefinition();
    } else {
      AUDIO_NODE_DEFINITION = require("devtools/server/actors/utils/audionodes.json");
    }

    // Make sure the backend is prepared to handle audio contexts.
    // Since actors are created lazily on the first request to them, we need to send an
    // early request to ensure the CallWatcherActor is running and watching for new window
    // globals.
    gFront.setup({ reload: false });
  },

  /**
   * Remove events emitted by the current tab target.
   */
  destroy: function() {
    gTarget.off("will-navigate", this._onTabWillNavigate);
    gFront.off("start-context", this._onStartContext);
    gFront.off("create-node", this._onCreateNode);
    gFront.off("connect-node", this._onConnectNode);
    gFront.off("connect-param", this._onConnectParam);
    gFront.off("disconnect-node", this._onDisconnectNode);
    gFront.off("change-param", this._onChangeParam);
    gFront.off("destroy-node", this._onDestroyNode);
    this._prefObserver.off("devtools.theme", this._onThemeChange);
    this._prefObserver.destroy();
  },

  /**
   * Called when page is reloaded to show the reload notice and waiting
   * for an audio context notice.
   */
  reset: function() {
    $("#content").hidden = true;
    ContextView.resetUI();
    InspectorView.resetUI();
    PropertiesView.resetUI();
  },

  // Since node events (create, disconnect, connect) are all async,
  // we have to make sure to wait that the node has finished creating
  // before performing an operation on it.
  getNode: async function(nodeActor) {
    const id = nodeActor.actorID;
    let node = gAudioNodes.get(id);

    if (!node) {
      const { resolve, promise } = defer();
      gAudioNodes.on("add", function createNodeListener(createdNode) {
        if (createdNode.id === id) {
          gAudioNodes.off("add", createNodeListener);
          resolve(createdNode);
        }
      });
      node = await promise;
    }
    return node;
  },

  /**
   * Fired when the devtools theme changes (light, dark, etc.)
   * so that the graph can update marker styling, as that
   * cannot currently be done with CSS.
   */
  _onThemeChange: function() {
    const newValue = Services.prefs.getCharPref("devtools.theme");
    window.emit(EVENTS.THEME_CHANGE, newValue);
  },

  /**
   * Called for each location change in the debugged tab.
   */
  _onTabWillNavigate: function({isFrameSwitching}) {
    // Clear out current UI.
    this.reset();

    // When switching to an iframe, ensure displaying the reload button.
    // As the document has already been loaded without being hooked.
    if (isFrameSwitching) {
      $("#reload-notice").hidden = false;
      $("#waiting-notice").hidden = true;
    } else {
      // Otherwise, we are loading a new top level document,
      // so we don't need to reload anymore and should receive
      // new node events.
      $("#reload-notice").hidden = true;
      $("#waiting-notice").hidden = false;
    }

    // Clear out stored audio nodes
    gAudioNodes.reset();

    window.emit(EVENTS.UI_RESET);
  },

  /**
   * Called after the first audio node is created in an audio context,
   * signaling that the audio context is being used.
   */
  _onStartContext: function() {
    $("#reload-notice").hidden = true;
    $("#waiting-notice").hidden = true;
    $("#content").hidden = false;
    window.emit(EVENTS.START_CONTEXT);
  },

  /**
   * Called when a new node is created. Creates an `AudioNodeView` instance
   * for tracking throughout the editor.
   */
  _onCreateNode: function(nodeActor) {
    gAudioNodes.add(nodeActor);
  },

  /**
   * Called on `destroy-node` when an AudioNode is GC'd. Removes
   * from the AudioNode array and fires an event indicating the removal.
   */
  _onDestroyNode: function(nodeActor) {
    gAudioNodes.remove(gAudioNodes.get(nodeActor.actorID));
  },

  /**
   * Called when a node is connected to another node.
   */
  async _onConnectNode({ source: sourceActor, dest: destActor }) {
    const source = await WebAudioEditorController.getNode(sourceActor);
    const dest = await WebAudioEditorController.getNode(destActor);
    source.connect(dest);
  },

  /**
   * Called when a node is conneceted to another node's AudioParam.
   */
  async _onConnectParam({ source: sourceActor, dest: destActor, param }) {
    const source = await WebAudioEditorController.getNode(sourceActor);
    const dest = await WebAudioEditorController.getNode(destActor);
    source.connect(dest, param);
  },

  /**
   * Called when a node is disconnected.
   */
  async _onDisconnectNode(nodeActor) {
    const node = await WebAudioEditorController.getNode(nodeActor);
    node.disconnect();
  },

  /**
   * Called when a node param is changed.
   */
  async _onChangeParam({ actor, param, value }) {
    const node = await WebAudioEditorController.getNode(actor);
    window.emit(EVENTS.CHANGE_PARAM, node, param, value);
  },
};
