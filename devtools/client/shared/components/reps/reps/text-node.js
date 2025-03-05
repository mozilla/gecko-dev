/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  button,
  span,
} from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";

import {
  appendRTLClassNameIfNeeded,
  cropString,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";
import {
  rep as StringRep,
  isLongString,
} from "resource://devtools/client/shared/components/reps/reps/string.mjs";

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
    className: appendRTLClassNameIfNeeded("objectBox objectBox-textNode", text),
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

const rep = wrapRender(TextNode);

// Exports from this module
export { rep, supportsObject };
