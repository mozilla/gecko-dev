/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import React, {
  Component,
  createFactory,
} from "devtools/client/shared/vendor/react";
import { div, button } from "devtools/client/shared/vendor/react-dom-factories";
import SearchInput from "../shared/SearchInput";
import { connect } from "devtools/client/shared/vendor/react-redux";
import {
  getSelectedTraceIndex,
  getTopTraces,
  getAllTraces,
  getTraceChildren,
  getTraceParents,
  getTraceFrames,
  getAllMutationTraces,
  getAllTraceCount,
  getIsCurrentlyTracing,
  getRuntimeVersions,
} from "../../selectors/index";
const VirtualizedTree = require("resource://devtools/client/shared/components/VirtualizedTree.js");
const FrameView = createFactory(
  require("resource://devtools/client/shared/components/Frame.js")
);
const {
  TRACER_FIELDS_INDEXES,
} = require("resource://devtools/server/actors/tracer.js");
const {
  HTMLTooltip,
} = require("resource://devtools/client/shared/widgets/tooltip/HTMLTooltip.js");

import actions from "../../actions/index";

const isMacOS = AppConstants.platform == "macosx";
const TREE_NODE_HEIGHT = 20;
const DEBUG = false;

export class Tracer extends Component {
  constructor(props) {
    super(props);

    this.state = {
      // List of expanded traces in the tree
      expanded: new Set(),

      // First visible trace's index.
      // Note that these two indexes aren't related to the VirtualizedTree viewport.
      // That's the possibly visible traces when scrolling top/bottom of the whole Tree.
      startIndex: 0,

      // Last visible trace's index. -1 is we should show all of them at the end.
      // As soon as we start scrolling via the left slider, new traces are added outside of the selected viewport.
      endIndex: -1,

      // Number of trace rendered in the timeline and the tree (considering the tree is expanded, less may be displayed based on collapsing)
      renderedTraceCount: 0,
    };

    this.onSliderClick = this.onSliderClick.bind(this);
    this.onSliderWheel = this.onSliderWheel.bind(this);
    this.resetZoom = this.resetZoom.bind(this);
  }

  UNSAFE_componentWillReceiveProps(nextProps) {
    const { traceParents } = this.props;
    if (
      nextProps.selectedTraceIndex != this.props.selectedTraceIndex &&
      nextProps.selectedTraceIndex != null
    ) {
      const { expanded } = this.state;
      let index = traceParents[nextProps.selectedTraceIndex];
      while (index) {
        expanded.add(index);
        index = traceParents[index];
      }
      this.setState({ expanded });
    }

    // Force update the renderedTraceCount when we receive new traces
    if (nextProps.traceCount != this.props.traceCount) {
      if (nextProps.traceCount == 0) {
        // Reset the indexes when the view is cleared (i.e. when we just started recording a new trace)
        this.updateIndexes(
          {
            startIndex: 0,
            endIndex: -1,
          },
          nextProps
        );
      } else {
        this.updateIndexes(
          {
            startIndex: this.state.startIndex,
            endIndex: this.state.endIndex,
          },
          nextProps
        );
      }
    }
  }

  componentDidMount() {
    // Prevent toolbox zooming when using Ctrl+Wheel on the slider.
    // (for some reason... React doesn't seem to register the right "wheel", event listener via `onWheel`,
    //  which is the one to cancel to prevent toolbox zooming code to work.)
    this.refs.timeline.onwheel = e => e.preventDefault();

    if (!this.tooltip) {
      this.instantiateTooltip();
    }
  }

  instantiateTooltip() {
    this.tooltip = new HTMLTooltip(this.refs.timeline.ownerDocument, {
      className: "event-tooltip",
      type: "arrow",
      // Avoid consuming the first click on the anchored UI element in the slider
      consumeOutsideClicks: false,
    });
    this.tooltip.setContentSize({ height: "auto" });
    this.tooltip.startTogglingOnHover(this.refs.timeline, (target, tooltip) => {
      if (target.classList.contains("tracer-slider-event")) {
        const { traceIndex } = target.dataset;
        const trace = this.props.allTraces[traceIndex];
        const eventName = trace[TRACER_FIELDS_INDEXES.EVENT_NAME];
        const eventType = getEventClassNameFromTraceEventName(eventName);
        tooltip.panel.innerHTML = "";
        const el = document.createElement("div");
        el.classList.add("tracer-dom-event", eventType);
        el.textContent = `DOM | ${eventName}`;
        tooltip.panel.append(
          el,
          document.createElement("hr"),
          document.createTextNode(
            "Double click to focus on the executions related to this event."
          )
        );
        return true;
      } else if (target.classList.contains("tracer-slider-mutation")) {
        const { traceIndex } = target.dataset;
        const trace = this.props.allTraces[traceIndex];
        const mutationType = trace[TRACER_FIELDS_INDEXES.DOM_MUTATION_TYPE];
        tooltip.panel.innerHTML = "";
        const el = document.createElement("div");
        el.classList.add("tracer-dom-mutation");
        el.textContent = `DOM Mutation | ${mutationType}`;
        tooltip.panel.append(
          el,
          document.createElement("hr"),
          document.createTextNode(
            "Click to find the call tree leading to this mutation."
          )
        );
        return true;
      }
      return false;
    });
  }

  componentDidUpdate() {
    if (DEBUG) {
      dump(
        ` # start: ${this.state.startIndex} end: ${this.state.endIndex} rendered: ${this.state.renderedTraceCount} traceCount:${this.props.traceCount}\n`
      );
    }
  }

  renderTree() {
    let {
      selectedTraceIndex,
      topTraces,
      allTraces,
      traceChildren,
      traceParents,
    } = this.props;
    if (!topTraces.length) {
      if (!this.props.isTracing) {
        return div(
          { className: "tracer-message" },
          "Tracer is off, or pending for next interaction/load."
        );
      }
      return div(
        { className: "tracer-message" },
        "Waiting for the first JavaScript executions"
      );
    }

    const { searchStrings } = this.state;

    // Indexes are floating number, so convert them to a decimal number as indexes in the trace array
    let { startIndex, endIndex } = this.state;
    startIndex = Math.floor(startIndex);
    endIndex = Math.floor(endIndex);

    if (startIndex != 0 || endIndex != -1) {
      // When we start zooming, only consider traces whose top level frame
      // is in the zoomed section.

      // Lookup for the first top trace after the start index
      let topTracesStartIndex = 0;
      if (startIndex != 0) {
        topTracesStartIndex = -1;
        for (let i = 0; i < topTraces.length; i++) {
          const traceIndex = topTraces[i];
          if (traceIndex >= startIndex) {
            topTracesStartIndex = i;
            break;
          }
        }
      }

      // Lookup for the first top trace from the end before the end index
      let topTracesEndIndex = topTraces.length;
      if (endIndex != -1) {
        for (let i = topTraces.length; i >= 0; i--) {
          const traceIndex = topTraces[i];
          if (traceIndex <= endIndex) {
            topTracesEndIndex = i + 1;
            break;
          }
        }
      }

      if (topTracesStartIndex == -1) {
        // When none of the top traces are within the selected range, pick the start index of top trace.
        // This happens when we zoom on the last call tree at the end of the record.
        topTraces = [startIndex];
      } else {
        topTraces = topTraces.slice(topTracesStartIndex, topTracesEndIndex);
      }

      // When the top trace isn't the top most one (`!0`) and isn't a top trace (`!topTraces[0]`),
      // We need to add the current start trace as a top trace, as well as all its following siblings
      // and the following siblings of parent traces recursively.
      // This help show partial call tree when scrolling/zooming with a partial view on a call stack.
      //
      // Note that for endIndex, the cut is being done in VirtualizedTree's getChildren function.
      if (startIndex != 0 && topTraces[0] != startIndex) {
        const results = [];
        results.push(startIndex);
        collectAllSiblings(traceParents, traceChildren, startIndex, results);
        topTraces.unshift(...results);
      }
    }

    if (searchStrings) {
      topTraces = topTraces.filter(traceIndex => {
        const trace = allTraces[traceIndex];
        if (trace[TRACER_FIELDS_INDEXES.TYPE] != "event") {
          return false;
        }
        let label = trace[TRACER_FIELDS_INDEXES.EVENT_NAME];
        if (!label) {
          return false;
        }
        label = label.toLowerCase();
        return searchStrings.some(search => label.includes(search));
      });
      if (!topTraces.length) {
        return div(
          { className: "tracer-message" },
          "No trace matches for the current search"
        );
      }
    }

    return React.createElement(VirtualizedTree, {
      itemHeight: TREE_NODE_HEIGHT,
      autoExpandDepth: 1,
      getRoots() {
        return topTraces;
      },
      getKey(traceIndex) {
        return `${traceIndex}`;
      },
      getParent(traceIndex) {
        return traceParents[traceIndex];
      },
      getChildren(traceIndex) {
        // When we aren't displaying all children up to the end of the record,
        // we may need to remove children that are outside of the viewport.
        if (endIndex != -1) {
          return traceChildren[traceIndex].filter(index => {
            return index < endIndex;
          });
        }
        return traceChildren[traceIndex];
      },

      isExpanded: traceIndex => {
        return this.state.expanded.has(traceIndex);
      },
      onExpand: traceIndex => {
        const { expanded } = this.state;
        expanded.add(traceIndex);
        this.setState({ expanded });
      },
      onCollapse: traceIndex => {
        const { expanded } = this.state;
        expanded.delete(traceIndex);
        this.setState({ expanded });
      },

      focused: selectedTraceIndex,
      onFocus: traceIndex => {
        this.props.selectTrace(traceIndex);
      },

      shown: selectedTraceIndex,

      renderItem: (traceIndex, _depth, isFocused, arrow, _isExpanded) => {
        const trace = allTraces[traceIndex];
        const type = trace[TRACER_FIELDS_INDEXES.TYPE];

        if (type == "event") {
          // Trace for DOM Events are always top level trace (and do not need margin/indent)
          const eventName = trace[TRACER_FIELDS_INDEXES.EVENT_NAME];

          const eventType = getEventClassNameFromTraceEventName(eventName);
          return div(
            {
              className: "trace-line",
            },
            arrow,
            div(
              {
                className: `tracer-dom-event ${eventType}${
                  selectedTraceIndex == traceIndex ? " selected" : ""
                }`,

                onDoubleClick: () => {
                  this.focusOnTrace(traceIndex);
                },
              },
              `DOM | ${eventName}`
            )
          );
        }

        if (type == "dom-mutation") {
          // Trace for DOM Mutations are always a leaf and don't have children.
          const mutationType = trace[TRACER_FIELDS_INDEXES.DOM_MUTATION_TYPE];
          return div(
            {
              className: `tracer-dom-mutation${
                selectedTraceIndex == trace ? " selected" : ""
              }`,
            },
            `DOM Mutation | ${mutationType}`
          );
        }

        if (type == "exit") {
          return null;
        }

        let className = "";
        if (selectedTraceIndex) {
          let idx = selectedTraceIndex;
          let onStack = false;
          while ((idx = traceParents[idx])) {
            if (idx == traceIndex) {
              onStack = true;
              break;
            }
          }
          if (onStack) {
            className += " onstack";
          }
        }
        const frameIndex = trace[TRACER_FIELDS_INDEXES.FRAME_INDEX];
        const frame = this.props.frames[frameIndex];
        return div(
          {
            className: "trace-line",
            onDoubleClick: () => {
              this.focusOnTrace(traceIndex);
            },
          },
          arrow,
          FrameView({
            className,
            showFunctionName: true,
            showAnonymousFunctionName: true,
            frame,
            sourceMapURLService: window.sourceMapURLService,
          })
        );
      },
    });
  }

  onSliderClick(event) {
    const { top, height } = this.refs.sliceSlider.getBoundingClientRect();
    const yInSlider = event.clientY - top;
    const mousePositionRatio = yInSlider / height;

    // Indexes and ratios are floating number whereas
    // we expect to pass an array index to `selectTrace`.
    const index = Math.round(
      this.state.startIndex + mousePositionRatio * this.state.renderedTraceCount
    );

    this.props.selectTrace(index);
  }

  searchInputOnChange = e => {
    // Support multiple search strings being comma separated,
    // ignore any extra white space and do a case insensitive search,
    // ignore empty search strings,
    const searchStrings = e.target.value
      .split(",")
      .map(search => search.trim().toLowerCase())
      .filter(search => !!search.length);
    if (searchStrings.length) {
      this.setState({
        searchStrings,
      });
    } else {
      this.setState({
        searchStrings: null,
      });
    }
  };

  renderSearchInput() {
    return React.createElement(SearchInput, {
      query: this.state.query,
      count: 0,
      placeholder: "Search DOM Events (comma separated list)",
      size: "small",
      showErrorEmoji: false,
      isLoading: false,
      onChange: this.searchInputOnChange,
      onKeyDown: () => {},
      showClose: false,
      showExcludePatterns: false,
      showSearchModifiers: false,
      searchOptions: {},
    });
  }

  onSliderWheel(event) {
    const direction = event.deltaY > 0 ? 1 : -1;
    const scrolledDelta = Math.abs(event.deltaY) * 0.01;

    let { startIndex, endIndex } = this.state;

    if (isMacOS ? event.metaKey : event.ctrlKey) {
      // Handle zooming it/out as we are either using CtrlOrMeta+Wheel or zooming via the touchpad

      // Compute the ratio (a percentage) of the position where the mouse or touch started zooming from
      const { top, height } = this.refs.sliceSlider.getBoundingClientRect();
      const yInSlider = event.clientY - top;
      const zoomOriginRatio = yInSlider / height;

      // Compute the number of indexes we should add or remove to both indexes
      const shift = Math.floor(
        Math.max(this.state.renderedTraceCount * scrolledDelta, 2) * direction
      );

      // Use the origin ratio in order to try to zoom where the cursor is
      // and distribute the shift between start and end according to its position.
      startIndex -= shift * zoomOriginRatio;
      if (endIndex == -1) {
        endIndex = this.props.traceCount + shift * (1 - zoomOriginRatio);
      } else {
        endIndex += shift * (1 - zoomOriginRatio);
      }
    } else {
      // Handle scrolling up/down as We are doing a simple scroll via wheel or touchpad

      // Avoid scrolling if we already at top or bottomn
      if (
        (direction < 0 && startIndex == 0) ||
        (direction > 0 && endIndex == -1)
      ) {
        return;
      }

      // Compute the number of indexes we should add or remove to both indexes
      const shift =
        Math.max(1, this.state.renderedTraceCount * scrolledDelta) * direction;
      startIndex += shift;
      if (endIndex == -1) {
        endIndex = this.props.traceCount + shift;
      } else {
        endIndex += shift;
      }
    }

    // Normalize the computed indexes.
    // start can't be lower than zero
    startIndex = Math.max(0, startIndex);
    // start can't be greater than the trace count
    startIndex = Math.min(startIndex, this.props.traceCount - 1);

    if (endIndex != -1) {
      // end can't be lower than start + 1
      endIndex = Math.max(startIndex + 1, endIndex);
      // end also can't be higher than the total number of traces
      if (endIndex >= this.props.traceCount) {
        // -1 means, there is no end filtering
        endIndex = -1;
      }
    }

    this.updateIndexes({
      startIndex,
      endIndex,
    });
  }

  updateIndexes({ startIndex, endIndex }, nextProps = this.props) {
    const renderedTraceCount =
      (endIndex == -1 ? nextProps.traceCount : endIndex) - startIndex;
    this.setState({
      startIndex,
      endIndex,
      renderedTraceCount,
    });
    if (this.tooltip) {
      this.tooltip.hide();
    }
  }

  focusOnTrace(traceIndex) {
    const lastTraceIndex = findLastTraceIndex(
      this.props.traceChildren,
      traceIndex
    );
    this.updateIndexes({
      startIndex: traceIndex,
      endIndex: lastTraceIndex,
    });
  }

  resetZoom() {
    this.updateIndexes({
      startIndex: 0,
      endIndex: -1,
    });
  }

  tracePositionInPercent(traceIndex) {
    return Math.round(
      ((traceIndex - this.state.startIndex) / this.state.renderedTraceCount) *
        100
    );
  }

  renderMutationsInSlider() {
    const { mutationTraces, allTraces } = this.props;
    const { startIndex, endIndex } = this.state;

    const displayedMutationTraces = [];
    for (const traceIndex of mutationTraces) {
      if (
        traceIndex >= startIndex &&
        (endIndex == -1 || traceIndex <= endIndex)
      ) {
        displayedMutationTraces.push(traceIndex);
      }
    }

    return displayedMutationTraces.map(traceIndex => {
      const symbol = {
        add: "+",
        attributes: "=",
        remove: "-",
      };
      const trace = allTraces[traceIndex];
      const mutationType = trace[TRACER_FIELDS_INDEXES.DOM_MUTATION_TYPE];
      return div(
        {
          className: `tracer-slider-mutation`,
          "data-trace-index": traceIndex,
          style: {
            top: `${this.tracePositionInPercent(traceIndex)}%`,
          },
          onClick: event => {
            event.preventDefault();
            event.stopPropagation();
            this.props.selectTrace(traceIndex);
          },
        },
        symbol[mutationType]
      );
    });
  }

  renderEventsInSlider() {
    const { topTraces, allTraces, traceChildren } = this.props;
    const { startIndex, endIndex } = this.state;

    const displayedTraceEvents = [];
    for (const traceIndex of topTraces) {
      // Match the last event index in order to allow showing partial event
      // which may not be complete at the beginning of the record when we are zoomed.
      const lastTraceIndex = findLastTraceIndex(traceChildren, traceIndex);
      if (
        lastTraceIndex >= startIndex &&
        (endIndex == -1 || traceIndex <= endIndex)
      ) {
        displayedTraceEvents.push(traceIndex);
      }
    }

    return displayedTraceEvents.map(traceIndex => {
      const eventPositionInPercent = this.tracePositionInPercent(traceIndex);
      const lastTraceIndex = findLastTraceIndex(traceChildren, traceIndex);
      const eventHeightInPercent = Math.round(
        ((lastTraceIndex - traceIndex) / this.state.renderedTraceCount) * 100
      );
      const trace = allTraces[traceIndex];
      if (trace[TRACER_FIELDS_INDEXES.TYPE] != "event") {
        return null;
      }
      const eventName = trace[TRACER_FIELDS_INDEXES.EVENT_NAME];
      const eventType = getEventClassNameFromTraceEventName(eventName);
      return div({
        className: `tracer-slider-event ${eventType}`,
        "data-trace-index": traceIndex,
        style: {
          top: `${eventPositionInPercent}%`,
          height: `${Math.max(
            Math.min(eventHeightInPercent, 100 - eventPositionInPercent),
            1
          )}%`,
        },
        onClick: event => {
          event.preventDefault();
          event.stopPropagation();
          this.props.selectTrace(traceIndex);
        },
        onDoubleClick: () => {
          this.focusOnTrace(traceIndex);
        },
      });
    });
  }

  renderVerticalSliders() {
    if (!this.props.traceCount) {
      // Always return the top element so that componentDidMount can register its wheel listener
      return div({
        className: "tracer-timeline hidden",
        ref: "timeline",
        onWheel: this.onSliderWheel,
      });
    }

    const { selectedTraceIndex } = this.props;

    const { startIndex, endIndex } = this.state;

    let selectedHighlightHeight;
    if (selectedTraceIndex > startIndex + this.state.renderedTraceCount) {
      selectedHighlightHeight = 100;
    } else if (selectedTraceIndex < startIndex) {
      selectedHighlightHeight = 0;
    } else {
      selectedHighlightHeight = this.tracePositionInPercent(selectedTraceIndex);
    }

    const classnames = [];
    if (startIndex > 0) {
      classnames.push("cut-start");
    }
    if (endIndex != -1) {
      classnames.push("cut-end");
    }
    if (selectedTraceIndex) {
      if (selectedTraceIndex < startIndex) {
        classnames.push("selected-before");
      } else if (endIndex != -1 && selectedTraceIndex > endIndex) {
        classnames.push("selected-after");
      }
    }
    return div(
      {
        className: "tracer-timeline",
        ref: "timeline",
        onWheel: this.onSliderWheel,
      },
      div(
        {
          className: `tracer-slider-box ${classnames.join(" ")}`,
        },
        div(
          {
            className: "tracer-slice-slider ",
            ref: "sliceSlider",
            onClick: this.onSliderClick,
            style: {
              "--slider-bar-progress": `${selectedHighlightHeight}%`,
            },
          },
          selectedTraceIndex
            ? div({
                className: "tracer-slider-bar",
              })
            : null,
          selectedTraceIndex &&
            selectedTraceIndex >= startIndex &&
            selectedTraceIndex <= startIndex + this.state.renderedTraceCount
            ? div({
                className: "tracer-slider-position",
              })
            : null,
          this.renderEventsInSlider(),
          this.renderMutationsInSlider()
        )
      )
    );
  }

  render() {
    const isZoomed = this.state.renderedTraceCount != this.props.traceCount;

    const { runtimeVersions } = this.props;

    return div(
      {
        className: "tracer-container",
        style: {
          "--tree-node-height": `${TREE_NODE_HEIGHT}px`,
        },
      },
      div(
        { className: "tracer-toolbar" },
        this.props.traceCount == 0
          ? div(
              {
                className: "tracer-experimental-notice",
              },
              "This panel is experimental. It may change, regress, be dropped or replaced."
            )
          : null,
        runtimeVersions &&
          runtimeVersions.localPlatformVersion !=
            runtimeVersions.remotePlatformVersion
          ? div(
              {
                className: "tracer-runtime-version-mismatch",
              },
              `Client and remote runtime have different versions (${runtimeVersions.localPlatformVersion} vs ${runtimeVersions.remotePlatformVersion}) . The Tracer may be broken because of protocol changes between these two versions. Please upgrade or downgrade one of the two to use the same major version.`
            )
          : null,
        this.renderSearchInput()
      ),
      isZoomed
        ? div(
            {
              className: "tracer-timeline-toolbar",
            },
            button(
              {
                className: "tracer-reset-zoom",
                onClick: this.resetZoom,
              },
              "Reset zoom"
            )
          )
        : null,
      this.renderVerticalSliders(),
      this.renderTree()
    );
  }
}

/**
 * Walk through the call tree to find the very last children frame
 * and return its trace index.
 *
 * @param {Object} traceChildren
 *                 The reducer data containing children trace indexes for all the traces.
 * @param {Number} traceIndex
 */
function findLastTraceIndex(traceChildren, traceIndex) {
  const children = traceChildren[traceIndex];
  if (!children.length) {
    return traceIndex;
  }
  return findLastTraceIndex(traceChildren, children.at(-1));
}

/**
 * Store in the `results` attribute all following siblings for a given trace,
 * as well as for its parents, that, recursively up to the top traces.
 *
 * @param {Object} traceParents
 *                 The reducer data containing parent trace index for all the traces.
 * @param {Object} traceChildren
 *                 The reducer data containing children trace indexes for all the traces.
 * @param {Number} traceIndex
 * @param {Array} results
 */
function collectAllSiblings(traceParents, traceChildren, traceIndex, results) {
  const parentIndex = traceParents[traceIndex];
  if (parentIndex != null) {
    const parentChildren = traceChildren[parentIndex];
    const indexInItsParent = parentChildren.indexOf(traceIndex);
    const siblingTraces = parentChildren.slice(indexInItsParent + 1);
    if (siblingTraces.length) {
      results.push(...siblingTraces);
    }
    collectAllSiblings(traceParents, traceChildren, parentIndex, results);
  }
}

/**
 * Given the TRACER_FIELDS_INDEXES.EVENT_NAME field of a trace,
 * return the classname to use for a given event trace.
 *
 * @param {String} eventName
 */
function getEventClassNameFromTraceEventName(eventName) {
  let eventType = "other";
  if (eventName.startsWith("mouse") || eventName.startsWith("click")) {
    eventType = "mouse";
  } else if (eventName.startsWith(" key")) {
    eventType = "key";
  }
  return eventType;
}

const mapStateToProps = state => {
  return {
    isTracing: getIsCurrentlyTracing(state),
    topTraces: getTopTraces(state),
    allTraces: getAllTraces(state),
    traceChildren: getTraceChildren(state),
    traceParents: getTraceParents(state),
    frames: getTraceFrames(state),
    mutationTraces: getAllMutationTraces(state),
    traceCount: getAllTraceCount(state),
    selectedTraceIndex: getSelectedTraceIndex(state),
    runtimeVersions: getRuntimeVersions(state),
  };
};

export default connect(mapStateToProps, {
  selectTrace: actions.selectTrace,
})(Tracer);
