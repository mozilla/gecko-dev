/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  button,
  span,
} from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";

import { wrapRender } from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";
import { Rep } from "resource://devtools/client/shared/components/reps/reps/rep.mjs";
import * as Grip from "resource://devtools/client/shared/components/reps/reps/grip.mjs";

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 */

Accessor.propTypes = {
  object: PropTypes.object.isRequired,
  mode: PropTypes.oneOf(Object.values(MODE)),
  shouldRenderTooltip: PropTypes.bool,
};

function Accessor(props) {
  const { object, evaluation, onInvokeGetterButtonClick, shouldRenderTooltip } =
    props;

  if (evaluation) {
    return span(
      {
        className: "objectBox objectBox-accessor objectTitle",
      },
      Rep({
        ...props,
        object: evaluation.getterValue,
        mode: props.mode || MODE.TINY,
        defaultRep: Grip,
      })
    );
  }

  if (hasGetter(object) && onInvokeGetterButtonClick) {
    return button({
      className: "invoke-getter",
      title: "Invoke getter",
      onClick: event => {
        onInvokeGetterButtonClick();
        event.stopPropagation();
      },
    });
  }

  const accessors = [];
  if (hasGetter(object)) {
    accessors.push("Getter");
  }

  if (hasSetter(object)) {
    accessors.push("Setter");
  }

  const accessorsString = accessors.join(" & ");

  return span(
    {
      className: "objectBox objectBox-accessor objectTitle",
      title: shouldRenderTooltip ? accessorsString : null,
    },
    accessorsString
  );
}

function hasGetter(object) {
  return object && object.get && object.get.type !== "undefined";
}

function hasSetter(object) {
  return object && object.set && object.set.type !== "undefined";
}

function supportsObject(object) {
  return hasGetter(object) || hasSetter(object);
}

const rep = wrapRender(Accessor);

// Exports from this module
export { rep, supportsObject };
