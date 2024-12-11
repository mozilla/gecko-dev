/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component, memo } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import AccessibleImage from "../../shared/AccessibleImage";
import { formatDisplayName } from "../../../utils/pause/frames/index";
import { getFileURL } from "../../../utils/source";
import FrameIndent from "./FrameIndent";
const classnames = require("resource://devtools/client/shared/classnames.js");

function FrameTitle({ frame, options = {}, l10n }) {
  const displayName = formatDisplayName(frame, options, l10n);
  return React.createElement(
    "span",
    {
      className: "title",
    },
    displayName
  );
}

FrameTitle.propTypes = {
  frame: PropTypes.object.isRequired,
  options: PropTypes.object.isRequired,
  l10n: PropTypes.object.isRequired,
};

function getFrameLocation(frame, shouldDisplayOriginalLocation) {
  if (shouldDisplayOriginalLocation) {
    return frame.location;
  }
  return frame.generatedLocation || frame.location;
}
const FrameLocation = memo(
  ({ frame, displayFullUrl = false, shouldDisplayOriginalLocation }) => {
    if (frame.library) {
      return React.createElement(
        "span",
        {
          className: "location",
        },
        frame.library,
        React.createElement(AccessibleImage, {
          className: `annotation-logo ${frame.library.toLowerCase()}`,
        })
      );
    }
    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const filename = displayFullUrl
      ? getFileURL(location.source, false)
      : location.source.shortName;
    return React.createElement(
      "span",
      {
        className: "location",
        title: location.source.url,
      },
      React.createElement(
        "span",
        {
          className: "filename",
        },
        filename
      ),
      ":",
      React.createElement(
        "span",
        {
          className: "line",
        },
        location.line
      )
    );
  }
);
FrameLocation.displayName = "FrameLocation";

FrameLocation.propTypes = {
  frame: PropTypes.object.isRequired,
  displayFullUrl: PropTypes.bool.isRequired,
};

export default class FrameComponent extends Component {
  static defaultProps = {
    hideLocation: false,
    shouldMapDisplayName: true,
    disableContextMenu: false,
  };

  static get propTypes() {
    return {
      disableContextMenu: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      frame: PropTypes.object.isRequired,
      getFrameTitle: PropTypes.func,
      hideLocation: PropTypes.bool.isRequired,
      isInGroup: PropTypes.bool,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectedFrame: PropTypes.object,
      isTracerFrameSelected: PropTypes.bool.isRequired,
      shouldMapDisplayName: PropTypes.bool.isRequired,
      shouldDisplayOriginalLocation: PropTypes.bool,
      showFrameContextMenu: PropTypes.func.isRequired,
    };
  }

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  get isDebugger() {
    return this.props.panel == "debugger";
  }

  render() {
    const {
      frame,
      selectedFrame,
      isTracerFrameSelected,
      hideLocation,
      shouldMapDisplayName,
      displayFullUrl,
      getFrameTitle,
      shouldDisplayOriginalLocation,
      isInGroup,
    } = this.props;
    const { l10n } = this.context;

    const isSelected =
      !isTracerFrameSelected && selectedFrame && selectedFrame.id === frame.id;

    const className = classnames("frame", {
      selected: isSelected,
      // When a JS Tracer frame is selected, the frame will still be considered as selected,
      // and switch from a blue to a grey background. It will still be considered as selected
      // from the point of view of stepping buttons.
      inactive:
        isTracerFrameSelected && selectedFrame && selectedFrame.id === frame.id,
      // Dead frames will likely not have inspectable scope
      dead: frame.state && frame.state !== "on-stack",
    });

    const location = getFrameLocation(frame, shouldDisplayOriginalLocation);
    const title = getFrameTitle
      ? getFrameTitle(`${getFileURL(location.source, false)}:${location.line}`)
      : undefined;

    return React.createElement(
      React.Fragment,
      null,
      frame.asyncCause &&
        React.createElement(
          "div",
          {
            className: "location-async-cause",
            tabIndex: -1,
            role: "presentation",
          },
          this.isSelectable && React.createElement(FrameIndent, null),
          this.isDebugger
            ? React.createElement(
                "span",
                {
                  className: "async-label",
                },
                frame.asyncCause
              )
            : l10n.getFormatStr("stacktrace.asyncStack", frame.asyncCause),
          this.isSelectable &&
            React.createElement("br", {
              className: "clipboard-only",
            })
        ),
      React.createElement(
        "div",
        {
          title,
          className,
          tabIndex: -1,
          role: "option",
          id: frame.id,
          "aria-selected": isSelected ? "true" : "false",
        },
        this.isSelectable &&
          React.createElement(FrameIndent, {
            indentLevel: isInGroup ? 2 : 1,
          }),
        React.createElement(FrameTitle, {
          frame,
          options: {
            shouldMapDisplayName,
          },
          l10n,
        }),
        !hideLocation &&
          React.createElement(
            "span",
            {
              className: "clipboard-only",
            },
            " "
          ),
        !hideLocation &&
          React.createElement(FrameLocation, {
            frame,
            displayFullUrl,
            shouldDisplayOriginalLocation,
          }),
        this.isSelectable &&
          React.createElement("br", {
            className: "clipboard-only",
          })
      )
    );
  }
}

FrameComponent.displayName = "Frame";
FrameComponent.contextTypes = { l10n: PropTypes.object };
