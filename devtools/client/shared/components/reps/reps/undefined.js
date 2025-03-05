/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";

import {
  getGripType,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders undefined value
 */

Undefined.propTypes = {
  shouldRenderTooltip: PropTypes.bool,
};

function Undefined(props) {
  const shouldRenderTooltip = props.shouldRenderTooltip;

  const config = getElementConfig(shouldRenderTooltip);

  return span(config, "undefined");
}

function getElementConfig(shouldRenderTooltip) {
  return {
    className: "objectBox objectBox-undefined",
    title: shouldRenderTooltip ? "undefined" : null,
  };
}

function supportsObject(object, noGrip = false) {
  if (noGrip === true) {
    return object === undefined;
  }

  return (
    (object && object.type && object.type == "undefined") ||
    getGripType(object, noGrip) == "undefined"
  );
}

const rep = wrapRender(Undefined);

// Exports from this module

export { rep, supportsObject };
