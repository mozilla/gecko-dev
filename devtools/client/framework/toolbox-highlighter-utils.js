/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const promise = require("promise");
const flags = require("devtools/shared/flags");

/**
 * Client-side highlighter shared module.
 * To be used by toolbox panels that need to highlight DOM elements.
 *
 * Highlighting and selecting elements is common enough that it needs to be at
 * toolbox level, accessible by any panel that needs it.
 * That's why the toolbox is the one that initializes the inspector and
 * highlighter. It's also why the API returned by this module needs a reference
 * to the toolbox which should be set once only.
 */

/**
 * Get the highighterUtils instance for a given toolbox.
 * This should be done once only by the toolbox itself and stored there so that
 * panels can get it from there. That's because the API returned has a stateful
 * scope that would be different for another instance returned by this function.
 *
 * @param {Toolbox} toolbox
 * @return {Object} the highlighterUtils public API
 */
exports.getHighlighterUtils = function(toolbox) {
  if (!toolbox) {
    throw new Error("Missing or invalid toolbox passed to getHighlighterUtils");
  }

  // Exported API properties will go here
  const exported = {};

  // Is the highlighter currently in pick mode
  let isPicking = false;

  // Is the box model already displayed, used to prevent dispatching
  // unnecessary requests, especially during toolbox shutdown
  let isNodeFrontHighlighted = false;

  /**
   * Release this utils, nullifying the references to the toolbox
   */
  exported.release = function() {
    toolbox = null;
  };

  /**
   * Make a function that initializes the inspector before it runs.
   * Since the init of the inspector is asynchronous, the return value will be
   * produced by Task.async and the argument should be a generator
   * @param {Function*} generator A generator function
   * @return {Function} A function
   */
  let isInspectorInitialized = false;
  const requireInspector = generator => {
    return async function(...args) {
      if (!isInspectorInitialized) {
        await toolbox.initInspector();
        isInspectorInitialized = true;
      }
      return generator.apply(null, args);
    };
  };

  /**
   * Start/stop the element picker on the debuggee target.
   * @param {Boolean} doFocus - Optionally focus the content area once the picker is
   *                            activated.
   * @return A promise that resolves when done
   */
  exported.togglePicker = function(doFocus) {
    if (isPicking) {
      return cancelPicker();
    }
    return startPicker(doFocus);
  };

  /**
   * Start the element picker on the debuggee target.
   * This will request the inspector actor to start listening for mouse events
   * on the target page to highlight the hovered/picked element.
   * Depending on the server-side capabilities, this may fire events when nodes
   * are hovered.
   * @param {Boolean} doFocus - Optionally focus the content area once the picker is
   *                            activated.
   * @return A promise that resolves when the picker has started or immediately
   * if it is already started
   */
  const startPicker = exported.startPicker =
    requireInspector(async function(doFocus = false) {
      if (isPicking) {
        return;
      }
      isPicking = true;

      toolbox.pickerButton.isChecked = true;
      await toolbox.selectTool("inspector", "inspect_dom");
      toolbox.on("select", cancelPicker);

      toolbox.walker.on("picker-node-hovered", onPickerNodeHovered);
      toolbox.walker.on("picker-node-picked", onPickerNodePicked);
      toolbox.walker.on("picker-node-previewed", onPickerNodePreviewed);
      toolbox.walker.on("picker-node-canceled", onPickerNodeCanceled);

      await toolbox.highlighter.pick(doFocus);
      toolbox.emit("picker-started");
    });

  /**
   * Stop the element picker. Note that the picker is automatically stopped when
   * an element is picked
   * @return A promise that resolves when the picker has stopped or immediately
   * if it is already stopped
   */
  const stopPicker = exported.stopPicker = requireInspector(async function() {
    if (!isPicking) {
      return;
    }
    isPicking = false;

    toolbox.pickerButton.isChecked = false;

    await toolbox.highlighter.cancelPick();
    toolbox.walker.off("picker-node-hovered", onPickerNodeHovered);
    toolbox.walker.off("picker-node-picked", onPickerNodePicked);
    toolbox.walker.off("picker-node-previewed", onPickerNodePreviewed);
    toolbox.walker.off("picker-node-canceled", onPickerNodeCanceled);

    toolbox.off("select", cancelPicker);
    toolbox.emit("picker-stopped");
  });

  /**
   * Stop the picker, but also emit an event that the picker was canceled.
   */
  const cancelPicker = exported.cancelPicker = async function() {
    await stopPicker();
    toolbox.emit("picker-canceled");
  };

  /**
   * When a node is hovered by the mouse when the highlighter is in picker mode
   * @param {Object} data Information about the node being hovered
   */
  function onPickerNodeHovered(data) {
    toolbox.emit("picker-node-hovered", data.node);
  }

  /**
   * When a node has been picked while the highlighter is in picker mode
   * @param {Object} data Information about the picked node
   */
  function onPickerNodePicked(data) {
    toolbox.selection.setNodeFront(data.node, { reason: "picker-node-picked" });
    stopPicker();
  }

  /**
   * When a node has been shift-clicked (previewed) while the highlighter is in
   * picker mode
   * @param {Object} data Information about the picked node
   */
  function onPickerNodePreviewed(data) {
    toolbox.selection.setNodeFront(data.node, { reason: "picker-node-previewed" });
  }

  /**
   * When the picker is canceled, stop the picker, and make sure the toolbox
   * gets the focus.
   */
  function onPickerNodeCanceled() {
    cancelPicker();
    toolbox.win.focus();
  }

  /**
   * Show the box model highlighter on a node in the content page.
   * The node needs to be a NodeFront, as defined by the inspector actor
   * @see devtools/server/actors/inspector/inspector.js
   * @param {NodeFront} nodeFront The node to highlight
   * @param {Object} options
   * @return A promise that resolves when the node has been highlighted
   */
  const highlightNodeFront = exported.highlightNodeFront = requireInspector(
  async function(nodeFront, options = {}) {
    if (!nodeFront) {
      return;
    }

    isNodeFrontHighlighted = true;
    await toolbox.highlighter.showBoxModel(nodeFront, options);

    toolbox.emit("node-highlight", nodeFront);
  });

  /**
   * This is a convenience method in case you don't have a nodeFront but a
   * valueGrip. This is often the case with VariablesView properties.
   * This method will simply translate the grip into a nodeFront and call
   * highlightNodeFront, so it has the same signature.
   * @see highlightNodeFront
   */
  exported.highlightDomValueGrip =
    requireInspector(async function(valueGrip, options = {}) {
      const nodeFront = await gripToNodeFront(valueGrip);
      if (nodeFront) {
        await highlightNodeFront(nodeFront, options);
      } else {
        throw new Error("The ValueGrip passed could not be translated to a NodeFront");
      }
    });

  /**
   * Translate a debugger value grip into a node front usable by the inspector
   * @param {ValueGrip}
   * @return a promise that resolves to the node front when done
   */
  const gripToNodeFront = exported.gripToNodeFront = requireInspector(
  async function(grip) {
    return toolbox.walker.getNodeActorFromObjectActor(grip.actor);
  });

  /**
   * Hide the highlighter.
   * @param {Boolean} forceHide Only really matters in test mode (when
   * flags.testing is true). In test mode, hovering over several nodes
   * in the markup view doesn't hide/show the highlighter to ease testing. The
   * highlighter stays visible at all times, except when the mouse leaves the
   * markup view, which is when this param is passed to true
   * @return a promise that resolves when the highlighter is hidden
   */
  exported.unhighlight = async function(forceHide = false) {
    forceHide = forceHide || !flags.testing;

    if (isNodeFrontHighlighted && forceHide && toolbox.highlighter) {
      isNodeFrontHighlighted = false;
      await toolbox.highlighter.hideBoxModel();
    }

    // unhighlight is called when destroying the toolbox, which means that by
    // now, the toolbox reference might have been nullified already.
    if (toolbox) {
      toolbox.emit("node-unhighlight");
    }
  };

  /**
   * If the main, box-model, highlighter isn't enough, or if multiple highlighters
   * are needed in parallel, this method can be used to return a new instance of a
   * highlighter actor, given a type.
   * The type of the highlighter passed must be known by the server.
   * The highlighter actor returned will have the show(nodeFront) and hide()
   * methods and needs to be released by the consumer when not needed anymore.
   * @return Promise a promise that resolves to the highlighter
   */
  exported.getHighlighterByType = requireInspector(async function(typeName) {
    const highlighter = await toolbox.inspector.getHighlighterByType(typeName);

    return highlighter || promise.reject("The target doesn't support " +
        `creating highlighters by types or ${typeName} is unknown`);
  });

  /**
   * Similar to getHighlighterByType, however it automatically memoizes and
   * destroys highlighters with the inspector, instead of having to be manually
   * managed by consumers.
   * The type of the highlighter passed must be known by the server.
   * The highlighter actor returned will have the show(nodeFront) and hide()
   * methods and needs to be released by the consumer when not needed anymore.
   * @return Promise a promise that resolves to the highlighter
   */
  exported.getOrCreateHighlighterByType = requireInspector(async function(typeName) {
    const highlighter = await toolbox.inspector.getOrCreateHighlighterByType(typeName);

    return highlighter || promise.reject("The target doesn't support " +
        `creating highlighters by types or ${typeName} is unknown`);
  });

  exported.getKnownHighlighter = function(typeName) {
    return toolbox.inspector && toolbox.inspector.getKnownHighlighter(typeName);
  };

  // Return the public API
  return exported;
};
