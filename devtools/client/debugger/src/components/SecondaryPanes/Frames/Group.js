/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import { getLibraryFromUrl } from "../../../utils/pause/frames/index";

import AccessibleImage from "../../shared/AccessibleImage";
import FrameComponent from "./Frame";
import Badge from "../../shared/Badge";
import FrameIndent from "./FrameIndent";

const classnames = require("resource://devtools/client/shared/classnames.js");

function FrameLocation({ frame, expanded }) {
  const library = frame.library || getLibraryFromUrl(frame);
  if (!library) {
    return null;
  }
  const arrowClassName = classnames("arrow", {
    expanded,
  });
  return React.createElement(
    "span",
    {
      className: "group-description",
    },
    React.createElement(AccessibleImage, {
      className: arrowClassName,
    }),
    React.createElement(AccessibleImage, {
      className: `annotation-logo ${library.toLowerCase()}`,
    }),
    React.createElement(
      "span",
      {
        className: "group-description-name",
      },
      library
    )
  );
}

FrameLocation.propTypes = {
  expanded: PropTypes.any.isRequired,
  frame: PropTypes.object.isRequired,
};

FrameLocation.displayName = "FrameLocation";

export default class Group extends Component {
  constructor(...args) {
    super(...args);
  }

  static get propTypes() {
    return {
      disableContextMenu: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      getFrameTitle: PropTypes.func,
      group: PropTypes.array.isRequired,
      groupTitle: PropTypes.string.isRequired,
      groupId: PropTypes.string.isRequired,
      expanded: PropTypes.bool.isRequired,
      frameIndex: PropTypes.number.isRequired,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectLocation: PropTypes.func,
      selectedFrame: PropTypes.object,
      isTracerFrameSelected: PropTypes.bool.isRequired,
      showFrameContextMenu: PropTypes.func.isRequired,
    };
  }

  get isSelectable() {
    return this.props.panel == "webconsole";
  }

  onContextMenu(event) {
    const { group } = this.props;
    const frame = group[0];
    this.props.showFrameContextMenu(event, frame, true);
  }

  renderFrames() {
    const {
      group,
      groupId,
      selectFrame,
      selectLocation,
      selectedFrame,
      isTracerFrameSelected,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      showFrameContextMenu,
      expanded,
    } = this.props;

    if (!expanded) {
      return null;
    }

    return React.createElement(
      "div",
      {
        className: "frames-list",
        role: "listbox",
        "aria-labelledby": groupId,
      },
      group.map((frame, index) =>
        React.createElement(FrameComponent, {
          frame,
          frameIndex: index,
          showFrameContextMenu,
          hideLocation: true,
          selectedFrame,
          isTracerFrameSelected,
          selectFrame,
          selectLocation,
          shouldMapDisplayName: false,
          displayFullUrl,
          getFrameTitle,
          disableContextMenu,
          panel,
          isInGroup: true,
        })
      )
    );
  }

  render() {
    const { l10n } = this.context;
    const { group, groupTitle, groupId, expanded, selectedFrame } = this.props;

    const isGroupFrameSelected = group.some(
      frame => frame.id == selectedFrame?.id
    );

    let l10NEntry;
    if (expanded) {
      if (isGroupFrameSelected) {
        l10NEntry = "callStack.group.collapseTooltipWithSelectedFrame";
      } else {
        l10NEntry = "callStack.group.collapseTooltip";
      }
    } else {
      l10NEntry = "callStack.group.expandTooltip";
    }

    const title = l10n.getFormatStr(l10NEntry, groupTitle);

    return React.createElement(
      React.Fragment,
      null,
      React.createElement(
        "div",
        {
          className: classnames("frames-group frame", {
            expanded,
          }),
          id: groupId,
          tabIndex: -1,
          role: "presentation",
          onClick: this.toggleFrames,
          title,
        },
        this.isSelectable && React.createElement(FrameIndent, null),
        React.createElement(FrameLocation, {
          frame: group[0],
          expanded,
        }),
        this.isSelectable &&
          React.createElement("span", { className: "clipboard-only" }, " "),
        React.createElement(Badge, { badgeText: this.props.group.length }),
        this.isSelectable &&
          React.createElement("br", {
            className: "clipboard-only",
          })
      ),
      this.renderFrames()
    );
  }
}

Group.displayName = "Group";
Group.contextTypes = { l10n: PropTypes.object };
