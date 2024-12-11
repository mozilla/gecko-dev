/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import { connect } from "devtools/client/shared/vendor/react-redux";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import FrameComponent from "./Frame";
import Group from "./Group";

import actions from "../../../actions/index";
import { collapseFrames } from "../../../utils/pause/frames/index";

import {
  getFrameworkGroupingState,
  getSelectedFrame,
  getCurrentThreadFrames,
  getShouldSelectOriginalLocation,
  getSelectedTraceIndex,
} from "../../../selectors/index";

const NUM_FRAMES_SHOWN = 7;

class Frames extends Component {
  constructor(props) {
    super(props);
    // This is used to cache the groups based on their group id's
    // easy access to simpler data structure. This was not put on
    // the state to avoid unnecessary updates.
    this.groups = {};

    this.state = {
      showAllFrames: !!props.disableFrameTruncate,
      currentFrame: "",
      expandedFrameGroups: this.props.expandedFrameGroups || {},
    };
  }

  static get propTypes() {
    return {
      disableContextMenu: PropTypes.bool.isRequired,
      disableFrameTruncate: PropTypes.bool.isRequired,
      displayFullUrl: PropTypes.bool.isRequired,
      frames: PropTypes.array.isRequired,
      frameworkGroupingOn: PropTypes.bool.isRequired,
      getFrameTitle: PropTypes.func,
      panel: PropTypes.oneOf(["debugger", "webconsole"]).isRequired,
      selectFrame: PropTypes.func.isRequired,
      selectLocation: PropTypes.func,
      selectedFrame: PropTypes.object,
      isTracerFrameSelected: PropTypes.bool.isRequired,
      showFrameContextMenu: PropTypes.func,
      shouldDisplayOriginalLocation: PropTypes.bool,
      onExpandFrameGroup: PropTypes.func,
      expandedFrameGroups: PropTypes.obj,
    };
  }

  shouldComponentUpdate(nextProps, nextState) {
    const {
      frames,
      selectedFrame,
      isTracerFrameSelected,
      frameworkGroupingOn,
      shouldDisplayOriginalLocation,
    } = this.props;

    const { showAllFrames, currentFrame, expandedFrameGroups } = this.state;
    return (
      frames !== nextProps.frames ||
      selectedFrame !== nextProps.selectedFrame ||
      isTracerFrameSelected !== nextProps.isTracerFrameSelected ||
      showAllFrames !== nextState.showAllFrames ||
      currentFrame !== nextState.currentFrame ||
      expandedFrameGroups !== nextState.expandedFrameGroups ||
      frameworkGroupingOn !== nextProps.frameworkGroupingOn ||
      shouldDisplayOriginalLocation !== nextProps.shouldDisplayOriginalLocation
    );
  }

  toggleFramesDisplay = () => {
    this.setState(prevState => ({
      showAllFrames: !prevState.showAllFrames,
    }));
  };

  isGroupExpanded(groupId) {
    return !!this.state.expandedFrameGroups[groupId];
  }

  expandGroup(el) {
    const { selectedFrame } = this.props;
    // No need to handles group frame checks for the smart trace
    if (selectedFrame) {
      // If a frame within the group is selected,
      // do not collapse the frame.
      const isGroupFrameSelected = this.groups[el.id].some(
        frame => frame.id == this.props.selectedFrame.id
      );

      if (this.isGroupExpanded(el.id) && isGroupFrameSelected) {
        return;
      }
    }

    const newExpandedGroups = {
      ...this.state.expandedFrameGroups,
      [el.id]: !this.state.expandedFrameGroups[el.id],
    };
    this.setState({ expandedFrameGroups: newExpandedGroups });
    // Cache the expanded state, for when the callstack is collapsed
    // expanded again later
    this.props.onExpandFrameGroup?.(newExpandedGroups);
  }

  collapseFrames(frames) {
    const { frameworkGroupingOn } = this.props;
    if (!frameworkGroupingOn) {
      return frames;
    }

    return collapseFrames(frames);
  }

  truncateFrames(frames) {
    const numFramesToShow = this.state.showAllFrames
      ? frames.length
      : NUM_FRAMES_SHOWN;

    return frames.slice(0, numFramesToShow);
  }

  onFocus(event) {
    event.stopPropagation();
    this.setState({ currentFrame: event.target.id });
  }

  onClick(event) {
    event.stopPropagation();

    const { frames } = this.props;
    const el = event.target.closest(".frame");
    // Ignore non frame elements and frame group title elements
    if (el == null) {
      return;
    }
    if (el.classList.contains("frames-group")) {
      this.expandGroup(el);
      return;
    }
    const clickedFrame = frames.find(frame => frame.id == el.id);
    this.props.selectFrame(clickedFrame);
  }

  // eslint-disable-next-line complexity
  onKeyDown(event) {
    const element = event.target;
    const focusedFrame = this.props.frames.find(
      frame => frame.id == element.id
    );
    const isFrameGroup = element.classList.contains("frames-group");
    const nextSibling = element.nextElementSibling;
    const previousSibling = element.previousElementSibling;
    if (event.key == "Tab") {
      if (!element.classList.contains("top-frames-list")) {
        event.preventDefault();
        element.closest(".top-frames-list").focus();
      }
    } else if (event.key == "Home") {
      this.focusFirstItem(event, previousSibling);
    } else if (event.key == "End") {
      this.focusLastItem(event, nextSibling);
    } else if (event.key == "Enter" || event.key == " ") {
      event.preventDefault();
      if (!isFrameGroup) {
        this.props.selectFrame(focusedFrame);
      } else {
        this.expandGroup(element);
      }
    } else if (event.key == "ArrowDown") {
      event.preventDefault();
      if (element.classList.contains("top-frames-list")) {
        element.firstChild.focus();
        return;
      }
      if (isFrameGroup) {
        if (nextSibling == null) {
          return;
        }
        if (nextSibling.classList.contains("frames-list")) {
          // If on an expanded frame group, jump to the first element inside the group
          nextSibling.firstChild.focus();
        } else if (!nextSibling.classList.contains("frame")) {
          // Jump any none frame elements e.g async frames
          nextSibling.nextElementSibling?.focus();
        } else {
          nextSibling.focus();
        }
      } else if (!isFrameGroup) {
        if (nextSibling == null) {
          const parentFrameGroup = element.closest(".frames-list");
          if (parentFrameGroup) {
            // Jump to the next item in the parent list if it exists
            parentFrameGroup.nextElementSibling?.focus();
          }
        } else if (!nextSibling.classList.contains("frame")) {
          // Jump any none frame elements e.g async frames
          nextSibling.nextElementSibling?.focus();
        } else {
          nextSibling.focus();
        }
      }
    } else if (event.key == "ArrowUp") {
      event.preventDefault();
      if (element.classList.contains("top-frames-list")) {
        element.lastChild.focus();
        return;
      }
      if (previousSibling == null) {
        const frameGroup = element.closest(".frames-list");
        if (frameGroup) {
          // Go to the heading of the frame group
          const frameGroupHeading = frameGroup.previousSibling;
          frameGroupHeading.focus();
        }
      } else if (previousSibling.classList.contains("frames-list")) {
        previousSibling.lastChild.focus();
      } else if (!previousSibling.classList.contains("frame")) {
        // Jump any none frame elements e.g async frames
        previousSibling.previousElementSibling?.focus();
      } else {
        previousSibling.focus();
      }
    }
  }

  focusFirstItem(event, previousSibling) {
    event.preventDefault();
    const element = event.target;
    const parent = element.parentNode;

    const isFrameList = parent.classList.contains("frames-list");
    // Already at the first element of the top list
    if (previousSibling == null && !isFrameList) {
      return;
    }

    if (isFrameList) {
      // Jump to the first frame in the main list
      parent.parentNode.firstChild.focus();
      return;
    }
    parent.firstChild.focus();
  }

  focusLastItem(event, nextSibling) {
    event.preventDefault();
    const element = event.target;
    const parent = element.parentNode;

    const isFrameList = parent.classList.contains("frames-list");
    // Already at the last element on the list
    if (nextSibling == null && !isFrameList) {
      return;
    }
    // If the last is an expanded frame group jump to
    // the last frame in the group.
    if (isFrameList) {
      // Jump to the last frame in the main list
      const parentLastItem = parent.parentNode.lastChild;
      if (parentLastItem && !parentLastItem.classList.contains("frames-list")) {
        parentLastItem.focus();
      } else {
        parent.lastChild.focus();
      }
    } else {
      const lastItem = element.parentNode.lastChild;
      if (lastItem.classList.contains("frames-list")) {
        lastItem.lastChild.focus();
      } else {
        lastItem.focus();
      }
    }
  }

  onContextMenu(event, frames) {
    event.stopPropagation();
    event.preventDefault();

    const el = event.target.closest("div[role='option'].frame");
    const currentFrame = frames.find(frame => frame.id == el.id);
    this.props.showFrameContextMenu(event, currentFrame);
  }

  renderFrames(frames) {
    const {
      selectFrame,
      selectLocation,
      selectedFrame,
      isTracerFrameSelected,
      displayFullUrl,
      getFrameTitle,
      disableContextMenu,
      panel,
      shouldDisplayOriginalLocation,
      showFrameContextMenu,
    } = this.props;

    const framesOrGroups = this.truncateFrames(this.collapseFrames(frames));

    // We're not using a <ul> because it adds new lines before and after when
    // the user copies the trace. Needed for the console which has several
    // places where we don't want to have those new lines.
    return React.createElement(
      "div",
      {
        className: "top-frames-list",
        onClick: e => this.onClick(e, selectedFrame),
        onKeyDown: e => this.onKeyDown(e),
        onFocus: e => this.onFocus(e),
        onContextMenu: disableContextMenu
          ? null
          : e => this.onContextMenu(e, frames),
        "aria-activedescendant": this.state.currentFrame,
        "aria-labelledby": "call-stack-pane",
        role: "listbox",
        tabIndex: 0,
      },
      framesOrGroups.map((frameOrGroup, index) => {
        if (frameOrGroup.id) {
          return React.createElement(FrameComponent, {
            frame: frameOrGroup,
            showFrameContextMenu,
            selectFrame,
            selectLocation,
            selectedFrame,
            isTracerFrameSelected,
            shouldDisplayOriginalLocation,
            key: String(frameOrGroup.id),
            displayFullUrl,
            getFrameTitle,
            disableContextMenu,
            panel,
            index,
          });
        }
        const groupTitle = frameOrGroup[0].library;
        const groupId = `${frameOrGroup[0].library}-${index}`;
        // Cache the group to use for checking when a group frame
        // is selected.
        this.groups[groupId] = frameOrGroup;
        return React.createElement(Group, {
          key: groupId,
          group: frameOrGroup,
          groupTitle,
          groupId,
          expanded: this.isGroupExpanded(groupId),
          frameIndex: index,
          showFrameContextMenu,
          selectFrame,
          selectLocation,
          selectedFrame,
          isTracerFrameSelected,
          displayFullUrl,
          getFrameTitle,
          disableContextMenu,
          panel,
          index,
        });
      })
    );
  }

  renderToggleButton(frames) {
    const { l10n } = this.context;
    const buttonMessage = this.state.showAllFrames
      ? l10n.getStr("callStack.collapse")
      : l10n.getStr("callStack.expand");

    frames = this.collapseFrames(frames);
    if (frames.length <= NUM_FRAMES_SHOWN) {
      return null;
    }
    return React.createElement(
      "div",
      {
        className: "show-more-container",
      },
      React.createElement(
        "button",
        {
          className: "show-more",
          onClick: this.toggleFramesDisplay,
        },
        buttonMessage
      )
    );
  }

  render() {
    const { frames, disableFrameTruncate } = this.props;

    if (!frames) {
      return React.createElement(
        "div",
        {
          className: "pane frames",
        },
        React.createElement(
          "div",
          {
            className: "pane-info empty",
          },
          L10N.getStr("callStack.notPaused")
        )
      );
    }
    return React.createElement(
      "div",
      {
        className: "pane frames",
      },
      this.renderFrames(frames),
      disableFrameTruncate ? null : this.renderToggleButton(frames)
    );
  }
}

Frames.contextTypes = { l10n: PropTypes.object };

const mapStateToProps = state => ({
  frames: getCurrentThreadFrames(state),
  frameworkGroupingOn: getFrameworkGroupingState(state),
  selectedFrame: getSelectedFrame(state),
  isTracerFrameSelected: getSelectedTraceIndex(state) != null,
  shouldDisplayOriginalLocation: getShouldSelectOriginalLocation(state),
  disableFrameTruncate: false,
  disableContextMenu: false,
  displayFullUrl: false,
});

export default connect(mapStateToProps, {
  selectFrame: actions.selectFrame,
  selectLocation: actions.selectLocation,
  showFrameContextMenu: actions.showFrameContextMenu,
})(Frames);

// Export the non-connected component in order to use it outside of the debugger
// panel (e.g. console, netmonitor, …).
export { Frames };
