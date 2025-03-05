/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint no-shadow: ["error", { "allow": ["name"] }] */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  getGripType,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import { rep as StringRep } from "resource://devtools/client/shared/components/reps/reps/string.mjs";

const MAX_STRING_LENGTH = 50;

/**
 * Renders a symbol.
 */

SymbolRep.propTypes = {
  object: PropTypes.object.isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function SymbolRep(props) {
  const {
    className = "objectBox objectBox-symbol",
    object,
    shouldRenderTooltip,
  } = props;
  const { name } = object;

  let symbolText = name || "";
  if (name && name !== "Symbol.iterator" && name !== "Symbol.asyncIterator") {
    symbolText = StringRep({
      object: symbolText,
      shouldCrop: true,
      cropLimit: MAX_STRING_LENGTH,
      useQuotes: true,
    });
  }

  const config = getElementConfig(
    {
      shouldRenderTooltip,
      className,
      name,
    },
    object
  );

  return span(config, "Symbol(", symbolText, ")");
}

function getElementConfig(opts, object) {
  const { shouldRenderTooltip, className, name } = opts;

  return {
    "data-link-actor-id": object.actor,
    className,
    title: shouldRenderTooltip ? `Symbol(${name})` : null,
  };
}

function supportsObject(object, noGrip = false) {
  return getGripType(object, noGrip) == "symbol";
}

const rep = wrapRender(SymbolRep);

// Exports from this module
export { rep, supportsObject };
