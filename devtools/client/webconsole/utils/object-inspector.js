/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");

const reps = require("devtools/client/shared/components/reps/reps");
const { REPS, MODE, objectInspector } = reps;
const ObjectInspector = createFactory(objectInspector.ObjectInspector);
const { Grip } = REPS;
const SmartTrace = createFactory(require("devtools/client/shared/components/SmartTrace"));

/**
 * Create and return an ObjectInspector for the given grip.
 *
 * @param {Object} grip
 *        The object grip to create an ObjectInspector for.
 * @param {Object} serviceContainer
 *        Object containing various utility functions
 * @param {Object} override
 *        Object containing props that should override the default props passed to
 *        ObjectInspector.
 * @returns {ObjectInspector}
 *        An ObjectInspector for the given grip.
 */
function getObjectInspector(grip, serviceContainer, override = {}) {
  let onDOMNodeMouseOver;
  let onDOMNodeMouseOut;
  let onInspectIconClick;

  if (serviceContainer) {
    onDOMNodeMouseOver = serviceContainer.highlightDomElement
      ? (object) => serviceContainer.highlightDomElement(object)
      : null;
    onDOMNodeMouseOut = serviceContainer.unHighlightDomElement;
    onInspectIconClick = serviceContainer.openNodeInInspector
      ? (object, e) => {
        // Stop the event propagation so we don't trigger ObjectInspector expand/collapse.
        e.stopPropagation();
        serviceContainer.openNodeInInspector(object);
      }
      : null;
  }

  const roots = createRootsFromGrip(grip);

  const objectInspectorProps = {
    autoExpandDepth: 0,
    mode: MODE.LONG,
    roots,
    onViewSourceInDebugger: serviceContainer.onViewSourceInDebugger,
    recordTelemetryEvent: serviceContainer.recordTelemetryEvent,
    openLink: serviceContainer.openLink,
    renderStacktrace: stacktrace => SmartTrace({
      stacktrace,
      onViewSourceInDebugger: serviceContainer
        ? serviceContainer.onViewSourceInDebugger || serviceContainer.onViewSource
        : null,
      onViewSourceInScratchpad: serviceContainer
        ? serviceContainer.onViewSourceInScratchpad || serviceContainer.onViewSource
        : null,
      sourceMapService: serviceContainer ? serviceContainer.sourceMapService : null,
    }),
  };

  if (!(typeof grip === "string" || (grip && grip.type === "longString"))) {
    Object.assign(objectInspectorProps, {
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      onInspectIconClick,
      defaultRep: Grip,
    });
  }

  if (override.autoFocusRoot) {
    Object.assign(objectInspectorProps, {
      focusedItem: roots[0],
    });
  }

  return ObjectInspector({...objectInspectorProps, ...override});
}

function createRootsFromGrip(grip) {
  return [{
    path: Symbol((grip && grip.actor) || JSON.stringify(grip)),
    contents: { value: grip },
  }];
}

module.exports = {
  getObjectInspector,
};
