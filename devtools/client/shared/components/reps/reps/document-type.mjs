/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint no-shadow: ["error", { "allow": ["DocumentType", "name"] }] */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  getGripType,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders DOM documentType object.
 */

DocumentTypeRep.propTypes = {
  object: PropTypes.object.isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function DocumentTypeRep(props) {
  const { object, shouldRenderTooltip } = props;
  const name =
    object && object.preview && object.preview.nodeName
      ? ` ${object.preview.nodeName}`
      : "";

  const config = getElementConfig({ object, shouldRenderTooltip, name });

  return span(config, `<!DOCTYPE${name}>`);
}

function getElementConfig(opts) {
  const { object, shouldRenderTooltip, name } = opts;

  return {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-document",
    title: shouldRenderTooltip ? `<!DOCTYPE${name}>` : null,
  };
}

// Registration
function supportsObject(object, noGrip = false) {
  return object?.preview && getGripType(object, noGrip) === "DocumentType";
}

const rep = wrapRender(DocumentTypeRep);

// Exports from this module
export { rep, supportsObject };
