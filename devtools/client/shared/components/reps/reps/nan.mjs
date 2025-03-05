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
 * Renders a NaN object
 */

NaNRep.PropTypes = {
  shouldRenderTooltip: PropTypes.bool,
};

function NaNRep(props) {
  const shouldRenderTooltip = props.shouldRenderTooltip;

  const config = getElementConfig(shouldRenderTooltip);

  return span(config, "NaN");
}

function getElementConfig(shouldRenderTooltip) {
  return {
    className: "objectBox objectBox-nan",
    title: shouldRenderTooltip ? "NaN" : null,
  };
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "NaN";
}

const rep = wrapRender(NaNRep);

// Exports from this module
export { rep, supportsObject };
