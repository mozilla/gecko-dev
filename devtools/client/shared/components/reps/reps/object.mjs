/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint no-shadow: ["error", { "allow": ["name"] }] */

import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import { span } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

import {
  wrapRender,
  ellipsisElement,
} from "resource://devtools/client/shared/components/reps/reps/rep-utils.mjs";
import PropRep from "resource://devtools/client/shared/components/reps/reps/prop-rep.mjs";
import { MODE } from "resource://devtools/client/shared/components/reps/reps/constants.mjs";

const DEFAULT_TITLE = "Object";

/**
 * Renders an object. An object is represented by a list of its
 * properties enclosed in curly brackets.
 *
 * This rep receives only an `object` property which is the actual object to render a
 * preview for.
 * This is used by JSON Viewer and Netmonitor to render JSON objects fetched from the
 * frontend, which doesn't involve any Object Actor/Front or any grip (which is the
 * object actor's form).
 * In the console and debugger, plain JS objects are rendered via GripRep (in grip.js),
 * as they involve an object actor.
 */

ObjectRep.propTypes = {
  object: PropTypes.object.isRequired,
  mode: PropTypes.oneOf(Object.values(MODE)),
  title: PropTypes.string,
  shouldRenderTooltip: PropTypes.bool,
};

function ObjectRep(props) {
  const object = props.object;
  const { shouldRenderTooltip = true } = props;

  if (props.mode === MODE.TINY) {
    const tinyModeItems = [];
    if (getTitle(props) !== DEFAULT_TITLE) {
      tinyModeItems.push(getTitleElement(props));
    } else {
      tinyModeItems.push(
        span(
          {
            className: "objectLeftBrace",
          },
          "{"
        ),
        Object.keys(object).length ? ellipsisElement : null,
        span(
          {
            className: "objectRightBrace",
          },
          "}"
        )
      );
    }

    return span(
      {
        className: "objectBox objectBox-object",
        title: shouldRenderTooltip ? getTitle(props) : null,
      },
      ...tinyModeItems
    );
  }

  const propsArray = safePropIterator(props, object);

  const showTitle = getTitle(props) !== DEFAULT_TITLE;
  const isEmptyObject = !propsArray.length;

  return span(
    {
      className: "objectBox objectBox-object",
      title: shouldRenderTooltip ? getTitle(props) : null,
    },
    showTitle ? getTitleElement(props) : null,
    span(
      {
        className: "objectLeftBrace",
      },
      (showTitle ? " " : "") + "{" + (isEmptyObject ? "" : " ")
    ),
    ...propsArray,
    span(
      {
        className: "objectRightBrace",
      },
      (isEmptyObject ? "" : " ") + "}"
    )
  );
}

function getTitleElement(props) {
  return span({ className: "objectTitle" }, getTitle(props));
}

function getTitle(props) {
  return props.title || DEFAULT_TITLE;
}

function safePropIterator(props, object, max) {
  max = typeof max === "undefined" ? 3 : max;
  try {
    return propIterator(props, object, max);
  } catch (err) {
    console.error(err);
  }
  return [];
}

function propIterator(props, object, max) {
  // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=945377
  if (Object.prototype.toString.call(object) === "[object Generator]") {
    object = Object.getPrototypeOf(object);
  }

  const elements = [];
  const unimportantProperties = [];
  let propertiesNumber = 0;
  const propertiesNames = Object.keys(object);

  const pushPropRep = (name, value) => {
    elements.push(
      PropRep({
        ...props,
        key: name,
        mode: MODE.TINY,
        name,
        object: value,
        equal: ": ",
      })
    );
    propertiesNumber++;

    if (propertiesNumber < propertiesNames.length) {
      elements.push(", ");
    }
  };

  try {
    for (const name of propertiesNames) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      // Object members with non-empty values are preferred since it gives the
      // user a better overview of the object.
      if (isInterestingProp(value)) {
        pushPropRep(name, value);
      } else {
        // If the property is not important, put its name on an array for later
        // use.
        unimportantProperties.push(name);
      }
    }
  } catch (err) {
    console.error(err);
  }

  if (propertiesNumber < max) {
    for (const name of unimportantProperties) {
      if (propertiesNumber >= max) {
        break;
      }

      let value;
      try {
        value = object[name];
      } catch (exc) {
        continue;
      }

      pushPropRep(name, value);
    }
  }

  if (propertiesNumber < propertiesNames.length) {
    elements.push(ellipsisElement);
  }

  return elements;
}

function isInterestingProp(value) {
  const type = typeof value;
  return type == "boolean" || type == "number" || (type == "string" && value);
}

function supportsObject(object, noGrip = false) {
  return noGrip;
}

const rep = wrapRender(ObjectRep);

// Exports from this module
export { rep, supportsObject };
