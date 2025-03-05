/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint no-shadow: ["error", { "allow": ["Document", "location"] }] */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  getGripType,
  getURLDisplayString,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";

/**
 * Renders DOM document object.
 */

DocumentRep.propTypes = {
  object: PropTypes.object.isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function DocumentRep(props) {
  const grip = props.object;
  const shouldRenderTooltip = props.shouldRenderTooltip;
  const location = getLocation(grip);
  const config = getElementConfig({ grip, location, shouldRenderTooltip });
  return span(
    config,
    getTitle(grip),
    location ? span({ className: "location" }, ` ${location}`) : null
  );
}

function getElementConfig(opts) {
  const { grip, location, shouldRenderTooltip } = opts;
  const config = {
    "data-link-actor-id": grip.actor,
    className: "objectBox objectBox-document",
  };

  if (!shouldRenderTooltip || !location) {
    return config;
  }
  config.title = `${grip.class} ${location}`;
  return config;
}

function getLocation(grip) {
  const location = grip.preview.location;
  return location ? getURLDisplayString(location) : null;
}

function getTitle(grip) {
  return span(
    {
      className: "objectTitle",
    },
    grip.class
  );
}

// Registration
function supportsObject(object, noGrip = false) {
  return object?.preview && getGripType(object, noGrip) === "HTMLDocument";
}

const rep = wrapRender(DocumentRep);

// Exports from this module
export { rep, supportsObject };
