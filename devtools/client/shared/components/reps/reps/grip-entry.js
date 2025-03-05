/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import { wrapRender } from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import PropRep from "resource://devtools/client/shared/components/reps/reps/prop-rep.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";

/**
 * Renders an entry of a Map, (Local|Session)Storage, Header or FormData entry.
 */
GripEntry.propTypes = {
  object: PropTypes.object,
  mode: PropTypes.oneOf(Object.values(MODE)),
  onDOMNodeMouseOver: PropTypes.func,
  onDOMNodeMouseOut: PropTypes.func,
  onInspectIconClick: PropTypes.func,
};

function GripEntry(props) {
  const { object } = props;

  let { key, value } = object.preview;
  if (key && key.getGrip) {
    key = key.getGrip();
  }
  if (value && value.getGrip) {
    value = value.getGrip();
  }

  return span(
    {
      className: "objectBox objectBox-map-entry",
    },
    PropRep({
      ...props,
      name: key,
      object: value,
      equal: " \u2192 ",
      title: null,
      suppressQuotes: false,
    })
  );
}

function supportsObject(grip, noGrip = false) {
  if (noGrip === true) {
    return false;
  }
  return (
    grip &&
    (grip.type === "formDataEntry" ||
      grip.type === "highlightRegistryEntry" ||
      grip.type === "mapEntry" ||
      grip.type === "storageEntry" ||
      grip.type === "urlSearchParamsEntry") &&
    grip.preview
  );
}

const rep = wrapRender(GripEntry);

// Exports from this module
export { rep, supportsObject };
