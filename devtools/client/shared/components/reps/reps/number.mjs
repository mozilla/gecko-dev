/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  getGripType,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders a number
 */

NumberRep.propTypes = {
  object: PropTypes.oneOfType([
    PropTypes.object,
    PropTypes.number,
    PropTypes.bool,
  ]).isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function NumberRep(props) {
  const value = stringify(props.object);
  const config = getElementConfig(props.shouldRenderTooltip, value);

  return span(config, value);
}

function stringify(object) {
  const isNegativeZero =
    Object.is(object, -0) || (object.type && object.type == "-0");

  return isNegativeZero ? "-0" : String(object);
}

function getElementConfig(shouldRenderTooltip, value) {
  return {
    className: "objectBox objectBox-number",
    title: shouldRenderTooltip ? value : null,
  };
}

const SUPPORTED_TYPES = new Set(["boolean", "number", "-0"]);
function supportsObject(object, noGrip = false) {
  return SUPPORTED_TYPES.has(getGripType(object, noGrip));
}

const rep = wrapRender(NumberRep);

// Exports from this module

export { rep, supportsObject };
