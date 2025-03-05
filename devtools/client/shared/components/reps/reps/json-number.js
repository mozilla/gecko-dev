/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import { JSON_NUMBER } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";

import { wrapRender } from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders a number that can't be parsed natively in JS. This is likely to happen
 * for large numbers that are serialized to JSON but are larger than Number.MAX_SAFE_INTEGER.
 * In such case, the JsonViewer will pass an object with a specific shape so we can
 * display it as in the original source, and indicate to the user that the value is special
 */

JsonNumber.propTypes = {
  object: PropTypes.oneOfType([
    PropTypes.object,
    PropTypes.number,
    PropTypes.bool,
  ]).isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function JsonNumber(props) {
  const value = props.object.source;
  const config = getElementConfig(props.shouldRenderTooltip, value);

  return span(
    config,
    span({ className: "source-value" }, value),
    span(
      {
        className: "parsed-value",
        title: "Javacript parsed value",
      },
      span({ className: "parsed-value-prefix" }, "JS:"),
      props.object.parsedValue
    )
  );
}

function getElementConfig(shouldRenderTooltip, value) {
  return {
    className: "objectBox objectBox-number objectBox-json-number",
    title: shouldRenderTooltip ? value : null,
  };
}

function supportsObject(object) {
  return object?.type === JSON_NUMBER;
}

const rep = wrapRender(JsonNumber);

export { rep, supportsObject };
