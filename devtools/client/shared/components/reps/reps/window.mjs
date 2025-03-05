/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint no-shadow: ["error", { "allow": ["location"] }] */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  getGripType,
  getURLDisplayString,
  wrapRender,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";

/**
 * Renders a grip representing a window.
 */

WindowRep.propTypes = {
  mode: PropTypes.oneOf(Object.values(MODE)),
  object: PropTypes.object.isRequired,
  shouldRenderTooltip: PropTypes.bool,
};

function WindowRep(props) {
  const { mode, object } = props;

  if (mode === MODE.TINY) {
    const tinyTitle = getTitle(object);
    const title = getTitle(object, true);
    const location = getLocation(object);
    const config = getElementConfig({ ...props, title, location });

    return span(
      config,
      span({ className: tinyTitle.className }, tinyTitle.content)
    );
  }

  const title = getTitle(object, true);
  const location = getLocation(object);
  const config = getElementConfig({ ...props, title, location });

  return span(
    config,
    span({ className: title.className }, title.content),
    span({ className: "location" }, location)
  );
}

function getElementConfig(opts) {
  const { object, shouldRenderTooltip, title, location } = opts;
  let tooltip;

  if (location) {
    tooltip = `${title.content}${location}`;
  } else {
    tooltip = `${title.content}`;
  }

  return {
    "data-link-actor-id": object.actor,
    className: "objectBox objectBox-Window",
    title: shouldRenderTooltip ? tooltip : null,
  };
}

function getTitle(object, trailingSpace) {
  let title = object.displayClass || object.class || "Window";
  if (trailingSpace === true) {
    title = `${title} `;
  }
  return {
    className: "objectTitle",
    content: title,
  };
}

function getLocation(object) {
  return getURLDisplayString(object.preview.url);
}

// Registration
function supportsObject(object, noGrip = false) {
  return object?.preview && getGripType(object, noGrip) == "Window";
}

const rep = wrapRender(WindowRep);

// Exports from this module
export { rep, supportsObject };
