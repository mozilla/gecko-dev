/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const {
    button,
    span,
  } = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
  const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

  // Reps
  const {
    appendRTLClassNameIfNeeded,
    cropString,
    wrapRender,
  } = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");
  const {
    MODE,
  } = require("resource://devtools/client/shared/components/reps/reps/constants.js");
  const {
    rep: StringRep,
    isLongString,
  } = require("resource://devtools/client/shared/components/reps/reps/string.js");

  /**
   * Renders DOM #text node.
   */

  TextNode.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function TextNode(props) {
    const { object: grip, mode = MODE.SHORT } = props;

    const isInTree = grip.preview && grip.preview.isConnected === true;
    const config = getElementConfig({ ...props, isInTree });
    const inspectIcon = getInspectIcon({ ...props, isInTree });

    if (mode === MODE.TINY || mode === MODE.HEADER) {
      return span(config, getTitle(grip), inspectIcon);
    }

    return span(
      config,
      getTitle(grip),
      " ",
      StringRep({
        className: "nodeValue",
        object: grip.preview.textContent,
      }),
      inspectIcon ? inspectIcon : null
    );
  }

  function getElementConfig(opts) {
    const {
      object,
      isInTree,
      onDOMNodeMouseOver,
      onDOMNodeMouseOut,
      shouldRenderTooltip,
    } = opts;

    const text = getTextContent(object);
    const config = {
      "data-link-actor-id": object.actor,
      "data-link-content-dom-reference": JSON.stringify(
        object.contentDomReference
      ),
      className: appendRTLClassNameIfNeeded(
        "objectBox objectBox-textNode",
        text
      ),
      title: shouldRenderTooltip ? `#text "${text}"` : null,
    };

    if (isInTree) {
      if (onDOMNodeMouseOver) {
        Object.assign(config, {
          onMouseOver: _ => onDOMNodeMouseOver(object),
        });
      }

      if (onDOMNodeMouseOut) {
        Object.assign(config, {
          onMouseOut: _ => onDOMNodeMouseOut(object),
        });
      }
    }

    return config;
  }

  function getTextContent(grip) {
    const text = grip.preview.textContent;
    return cropString(isLongString(text) ? text.initial : text);
  }

  function getInspectIcon(opts) {
    const { object, isInTree, onInspectIconClick } = opts;

    if (!isInTree || !onInspectIconClick) {
      return null;
    }

    return button({
      className: "open-inspector",
      draggable: false,
      // TODO: Localize this with "openNodeInInspector" when Bug 1317038 lands
      title: "Click to select the node in the inspector",
      onClick: e => onInspectIconClick(object, e),
    });
  }

  function getTitle() {
    const title = "#text";
    return span({}, title);
  }

  // Registration
  function supportsObject(grip) {
    return grip?.preview && grip?.class == "Text";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(TextNode),
    supportsObject,
  };
});
