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
 * Renders a BigInt Number
 */

BigIntRep.propTypes = {
  object: PropTypes.oneOfType([
    PropTypes.object,
    PropTypes.number,
    PropTypes.bool,
  ]).isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function BigIntRep(props) {
  const { object, shouldRenderTooltip } = props;
  const text = object.text;
  const config = getElementConfig({ text, shouldRenderTooltip });

  return span(config, `${text}n`);
}

function getElementConfig(opts) {
  const { text, shouldRenderTooltip } = opts;

  return {
    className: "objectBox objectBox-number",
    title: shouldRenderTooltip ? `${text}n` : null,
  };
}
function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) === "BigInt";
}

const rep = wrapRender(BigIntRep);

// Exports from this module

export { rep, supportsObject };
