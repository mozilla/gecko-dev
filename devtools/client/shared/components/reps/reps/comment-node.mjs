/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  cropString,
  cropMultipleLines,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";
import * as nodeConstants from "resource://devtools/client/shared/components/reps/shared/dom-node-constants.mjs";

/**
 * Renders DOM comment node.
 */

CommentNode.propTypes = {
  object: PropTypes.object.isRequired,
  mode: PropTypes.oneOf(Object.values(MODE)),
  shouldRenderTooltip: PropTypes.bool,
};

function CommentNode(props) {
  const { object, mode = MODE.SHORT, shouldRenderTooltip } = props;

  let { textContent } = object.preview;
  if (mode === MODE.TINY || mode === MODE.HEADER) {
    textContent = cropMultipleLines(textContent, 30);
  } else if (mode === MODE.SHORT) {
    textContent = cropString(textContent, 50);
  }

  const config = getElementConfig({
    object,
    textContent,
    shouldRenderTooltip,
  });

  return span(config, `<!-- ${textContent} -->`);
}

function getElementConfig(opts) {
  const { object, shouldRenderTooltip } = opts;

  // Run textContent through cropString to sanitize
  const uncroppedText = shouldRenderTooltip
    ? cropString(object.preview.textContent)
    : null;

  return {
    className: "objectBox theme-comment",
    "data-link-actor-id": object.actor,
    title: shouldRenderTooltip ? `<!-- ${uncroppedText} -->` : null,
  };
}

// Registration
function supportsObject(object) {
  return object?.preview?.nodeType === nodeConstants.COMMENT_NODE;
}

const rep = wrapRender(CommentNode);

// Exports from this module
export { rep, supportsObject };
