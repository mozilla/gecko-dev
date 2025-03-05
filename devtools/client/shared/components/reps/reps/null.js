/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";

import { wrapRender } from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders null value
 */

Null.PropTypes = {
  shouldRenderTooltip: PropTypes.bool,
};

function Null(props) {
  const shouldRenderTooltip = props.shouldRenderTooltip;

  const config = getElementConfig(shouldRenderTooltip);

  return span(config, "null");
}

function getElementConfig(shouldRenderTooltip) {
  return {
    className: "objectBox objectBox-null",
    title: shouldRenderTooltip ? "null" : null,
  };
}

function supportsObject(object, noGrip = false) {
  if (noGrip === true) {
    return object === null;
  }

  if (object && object.type && object.type == "null") {
    return true;
  }

  return object == null;
}

const rep = wrapRender(Null);

// Exports from this module

export { rep, supportsObject };
