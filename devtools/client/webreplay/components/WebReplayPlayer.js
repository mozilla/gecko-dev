/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { sortBy, range } = require("devtools/client/shared/vendor/lodash");

ChromeUtils.defineModuleGetter(
  this,
  "pointEquals",
  "resource://devtools/shared/execution-point-utils.js"
);

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);
const getFormatStr = (key, a) => L10N.getFormatStr(`toolbox.replay.${key}`, a);

const { div } = dom;

const markerWidth = 7;
const imgResource = "resource://devtools/client/debugger/images";
const imgChrome = "chrome://devtools/skin/images";
const shouldLog = false;

function classname(name, bools) {
  for (const key in bools) {
    if (bools[key]) {
      name += ` ${key}`;
    }
  }

  return name;
}

function log(message) {
  if (shouldLog) {
    console.log(message);
  }
}

function isError(message) {
  return message.source === "javascript" && message.level === "error";
}

function CommandButton({ img, className, onClick, active }) {
  const images = {
    rewind: "replay-resume",
    resume: "replay-resume",
    next: "next",
    previous: "next",
    pause: "replay-pause",
    play: "replay-resume",
  };

  const filename = images[img];
  const path = filename == "next" ? imgChrome : imgResource;
  const attrs = {
    className: classname(`command-button ${className}`, { active }),
    onClick,
  };

  if (active) {
    attrs.title = L10N.getStr(`toolbox.replay.${img}`);
  }

  return dom.div(
    attrs,
    dom.img({
      className: `btn ${img} ${className}`,
      style: {
        maskImage: `url("${path}/${filename}.svg")`,
      },
    })
  );
}

function getMessageProgress(message) {
  return getProgress(message.executionPoint);
}

function getProgress(executionPoint) {
  return executionPoint && executionPoint.progress;
}

function getClosestMessage(messages, executionPoint) {
  const progress = getProgress(executionPoint);

  return sortBy(messages, message =>
    Math.abs(progress - getMessageProgress(message))
  )[0];
}

function sameLocation(m1, m2) {
  const f1 = m1.frame;
  const f2 = m2.frame;

  return (
    f1.source === f2.source && f1.line === f2.line && f1.column === f2.column
  );
}

function getMessageLocation(message) {
  if (!message.frame) {
    return null;
  }
  const {
    frame: { source, line, column },
  } = message;
  return { sourceUrl: source, line, column };
}

const FirstCheckpointExecutionPoint = { checkpoint: 1, progress: 0 };

// Information about the progress and time at each checkpoint. This only grows,
// and is not part of the reducer store so we can update it without rerendering.
const gCheckpoints = [{ point: FirstCheckpointExecutionPoint, time: 0 }];

function executionPointTime({ checkpoint, progress, position }) {
  const info = gCheckpoints[checkpoint];
  if (!info) {
    // We might pause at a checkpoint before we've received its information.
    return recordingEndTime();
  }
  if (!position || !gCheckpoints[checkpoint + 1]) {
    return info.time;
  }
  const startProgress = info.point.progress;
  const nextInfo = gCheckpoints[checkpoint + 1];
  const fraction = (progress - startProgress) / (nextInfo.point.progress - startProgress);
  if (Number.isNaN(fraction)) {
    return info.time;
  }
  return info.time + fraction * (nextInfo.time - info.time);
}

function recordingEndTime() {
  return gCheckpoints[gCheckpoints.length - 1].time;
}

function similarPoints(p1, p2) {
  const time1 = executionPointTime(p1);
  const time2 = executionPointTime(p2);
  return Math.abs(time1 - time2) / recordingEndTime() < 0.001;
}

function binarySearch(start, end, callback) {
  while (start + 1 < end) {
    const mid = ((start + end) / 2) | 0;
    const rv = callback(mid);
    if (rv < 0) {
      end = mid;
    } else {
      start = mid;
    }
  }
  return start;
}

/*
 *
 * The player has 4 valid states
 * - Paused:       (paused, !recording, !seeking)
 * - Playing:      (!paused, !recording, !seeking)
 * - Seeking:      (!paused, !recording, seeking)
 * - Recording:    (!paused, recording, !seeking)
 *
 */
class WebReplayPlayer extends Component {
  static get propTypes() {
    return {
      toolbox: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.state = {
      executionPoint: FirstCheckpointExecutionPoint,
      recordingEndpoint: FirstCheckpointExecutionPoint,
      hoverPoint: null,
      seeking: false,
      recording: true,
      paused: false,
      messages: [],
      highlightedMessage: null,
      hoveredMessageOffset: null,
      unscannedRegions: [],
      cachedPoints: [],
      shouldAnimate: true,
      start: 0,
      end: 1,
    };

    this.lastPaint = null;
    this.hoveredMessage = null;
    this.overlayWidth = 1;

    this.onProgressBarClick = this.onProgressBarClick.bind(this);
    this.onProgressBarMouseOver = this.onProgressBarMouseOver.bind(this);
    this.onPlayerMouseLeave = this.onPlayerMouseLeave.bind(this);
  }

  componentDidMount() {
    this.overlayWidth = this.updateOverlayWidth();
    this.threadFront.on("paused", this.onPaused.bind(this));
    this.threadFront.on("resumed", this.onResumed.bind(this));
    this.threadFront.on("replayStatusUpdate", this.onStatusUpdate.bind(this));

    // Status updates normally only include deltas from the last status.
    // This will cause an update to be emitted with the full status.
    this.threadFront.replayFetchStatus();

    this.toolbox.getPanelWhenReady("webconsole").then(panel => {
      const consoleFrame = panel.hud.ui;
      consoleFrame.on("message-hover", this.onConsoleMessageHover.bind(this));
      consoleFrame.wrapper.subscribeToStore(this.onConsoleUpdate.bind(this));
    });
  }

  componentDidUpdate(prevProps, prevState) {
    this.overlayWidth = this.updateOverlayWidth();

    if (prevState.closestMessage != this.state.closestMessage) {
      this.scrollToMessage(this.state.closestMessage);
    }
  }

  get toolbox() {
    return this.props.toolbox;
  }

  get console() {
    return this.toolbox.getPanel("webconsole");
  }

  get threadFront() {
    return this.toolbox.threadFront;
  }

  isCached(message) {
    if (!message.executionPoint) {
      return false;
    }
    return this.state.cachedPoints.some(p => pointEquals(p, message.executionPoint));
  }

  isRecording() {
    return !this.isPaused() && this.state.recording;
  }

  isReplaying() {
    return !this.isPaused() && !this.state.recording;
  }

  isPaused() {
    return this.state.paused;
  }

  isSeeking() {
    return this.state.seeking;
  }

  getTickSize() {
    const { start, end } = this.state;
    const minSize = 10;

    if (!start && !end) {
      return minSize;
    }

    const maxSize = this.overlayWidth / 10;
    const ratio = end - start;
    return (1 - ratio) * maxSize + minSize;
  }

  getClosestMessage(point) {
    return getClosestMessage(this.state.messages, point);
  }

  getMousePosition(e) {
    const { start, end } = this.state;

    const { left, width } = e.currentTarget.getBoundingClientRect();
    const clickLeft = e.clientX;

    const clickPosition = (clickLeft - left) / width;
    return (end - start) * clickPosition + start;
  }

  paint(point) {
    if (point && this.lastPaint !== point) {
      this.lastPaint = point;
      this.threadFront.paint(point);
    }
  }

  onPaused(packet) {
    if (packet) {
      const { executionPoint } = packet;
      const closestMessage = this.getClosestMessage(executionPoint);

      const pausedMessage = this.state.messages
        .filter(message => message.executionPoint)
        .find(message => pointEquals(message.executionPoint, executionPoint));

      this.setState({
        executionPoint,
        paused: true,
        seeking: false,
        recording: false,
        closestMessage,
        pausedMessage,
      });
    }
  }

  onResumed(packet) {
    this.setState({ paused: false, closestMessage: null, pausedMessage: null });
  }

  onStatusUpdate({ status }) {
    const {
      recording,
      checkpoints,
      executionPoint,
      unscannedRegions,
      cachedPoints,
    } = status;

    const newState = {};

    if (recording !== undefined && recording != this.state.recording) {
      newState.recording = recording;
    }

    if (checkpoints !== undefined) {
      for (const { point, time } of checkpoints) {
        gCheckpoints[point.checkpoint] = { point, time };
      }

      const recordingEndpoint = checkpoints[checkpoints.length - 1].point;
      if (!similarPoints(recordingEndpoint, this.state.recordingEndpoint)) {
        this.state.recordingEndpoint = recordingEndpoint;
      }
    }

    if (executionPoint !== undefined) {
      newState.executionPoint = executionPoint;
    }

    if (unscannedRegions !== undefined) {
      let similar = unscannedRegions.length == this.state.unscannedRegions.length;
      if (similar) {
        for (let i = 0; i < unscannedRegions.length; i++) {
          const newRegion = unscannedRegions[i];
          const oldRegion = this.state.unscannedRegions[i];
          if (
            !similarPoints(newRegion.start, oldRegion.start) ||
            !similarPoints(newRegion.end, oldRegion.end) ||
            newRegion.traversed != oldRegion.traversed
          ) {
            similar = false;
            break;
          }
        }
      }

      if (!similar) {
        newState.unscannedRegions = unscannedRegions;
      }
    }

    if (cachedPoints !== undefined) {
      newState.cachedPoints = [
        ...this.state.cachedPoints,
        ...cachedPoints,
      ];
    }

    if (recording) {
      newState.shouldAnimate = true;
    }

    this.setState(newState);
  }

  onConsoleUpdate(consoleState) {
    const {
      messages: { visibleMessages, messagesById },
    } = consoleState;

    if (visibleMessages != this.state.visibleMessages) {
      let messages = visibleMessages
        .map(id => messagesById.get(id))
        .filter(message => message.source == "console-api" || isError(message));

      messages = sortBy(messages, message => getMessageProgress(message));

      this.setState({ messages, visibleMessages, shouldAnimate: false });
    }
  }

  onConsoleMessageHover(type, message) {
    if (type == "mouseleave") {
      return this.setState({ highlightedMessage: null });
    }

    if (type == "mouseenter") {
      return this.setState({ highlightedMessage: message.id });
    }

    return null;
  }

  setTimelinePosition({ position, direction }) {
    this.setState({ [direction]: position });
  }

  findMessage(message) {
    const consoleOutput = this.console.hud.ui.outputNode;
    return consoleOutput.querySelector(
      `.message[data-message-id="${message.id}"]`
    );
  }

  scrollToMessage(message) {
    if (!message) {
      return;
    }

    const element = this.findMessage(message);
    const consoleOutput = this.console.hud.ui.outputNode;

    if (element) {
      const consoleHeight = consoleOutput.getBoundingClientRect().height;
      const elementTop = element.getBoundingClientRect().top;
      if (elementTop < 30 || elementTop + 50 > consoleHeight) {
        element.scrollIntoView({ block: "center", behavior: "smooth" });
      }
    }
  }

  unhighlightConsoleMessage() {
    if (this.hoveredMessage) {
      this.hoveredMessage.classList.remove("highlight");
    }
  }

  highlightConsoleMessage(message) {
    if (!message) {
      return;
    }

    const element = this.findMessage(message);
    if (!element) {
      return;
    }

    this.unhighlightConsoleMessage();
    element.classList.add("highlight");
    this.hoveredMessage = element;
  }

  showMessage(message) {
    this.highlightConsoleMessage(message);
    this.scrollToMessage(message);
    this.paint(message.executionPoint);
  }

  onMessageMouseEnter(message, offset) {
    this.setState({ hoveredMessageOffset: offset });
    this.previewLocation(message);
    this.showMessage(message);
  }

  onMessageMouseLeave() {
    this.setState({ hoveredMessageOffset: null });
  }

  async previewLocation(closestMessage) {
    const dbg = await this.toolbox.loadTool("jsdebugger");
    const location = getMessageLocation(closestMessage);
    if (location) {
      dbg.previewPausedLocation(location);
    }
  }

  async clearPreviewLocation() {
    const dbg = await this.toolbox.loadTool("jsdebugger");
    dbg.clearPreviewPausedLocation();
  }

  onProgressBarClick(e) {
    if (e.altKey) {
      const direction = e.shiftKey ? "end" : "start";
      const position = this.getMousePosition(e);
      this.setTimelinePosition({ position, direction });
    } else {
      const { hoverPoint } = this.state;
      if (hoverPoint) {
        this.setState({ seeking: true });
        this.threadFront.timeWarp(hoverPoint);
      }
    }
  }

  onProgressBarMouseOver(e) {
    const { start, end, hoverPoint } = this.state;
    const mousePosition = this.getMousePosition(e);
    const time = (start + mousePosition * (end - start)) * recordingEndTime();

    let checkpoint = binarySearch(1, gCheckpoints.length, checkpoint => {
      return time - gCheckpoints[checkpoint].time;
    });

    // This gives us the most recent checkpoint, but use the next one instead
    // if we are closer to it.
    if (
      checkpoint + 1 < gCheckpoints.length &&
      Math.abs(time - gCheckpoints[checkpoint + 1].time) <
      Math.abs(time - gCheckpoints[checkpoint].time)
    ) {
      checkpoint++;
    }

    const point = gCheckpoints[checkpoint].point;
    if (!hoverPoint || !pointEquals(point, hoverPoint)) {
      this.threadFront.paint(point);
      this.setState({ hoverPoint: point });
    }
  }

  onPlayerMouseLeave() {
    this.unhighlightConsoleMessage();
    this.clearPreviewLocation();
    this.threadFront.paintCurrentPoint();

    if (this.state.hoverPoint) {
      this.setState({ hoverPoint: null });
    }
  }

  seek(executionPoint) {
    if (!executionPoint) {
      return null;
    }

    // set seeking to the current execution point to avoid a progress bar jump
    this.setState({ seeking: true });
    return this.threadFront.timeWarp(executionPoint);
  }

  next() {
    if (!this.isPaused()) {
      return null;
    }

    const { messages, executionPoint, recordingEndpoint } = this.state;
    let seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress > executionPoint.progress)
      .slice(0)[0];

    if (!seekPoint) {
      seekPoint = recordingEndpoint;
    }

    return this.seek(seekPoint);
  }

  async previous() {
    if (this.isRecording()) {
      await this.threadFront.interrupt();
    }

    if (!this.isPaused()) {
      return null;
    }

    const { messages, executionPoint } = this.state;

    const seekPoint = messages
      .map(m => m.executionPoint)
      .filter(point => point.progress < executionPoint.progress)
      .slice(-1)[0];

    return this.seek(seekPoint);
  }

  resume() {
    if (!this.isPaused()) {
      return null;
    }

    return this.threadFront.resume();
  }

  async rewind() {
    return this.threadFront.rewind();
  }

  pause() {
    if (this.isPaused()) {
      return null;
    }

    return this.threadFront.interrupt();
  }

  renderCommands() {
    const paused = this.isPaused();
    const recording = this.isRecording();
    const seeking = this.isSeeking();

    return [
      CommandButton({
        className: "",
        active: paused || recording,
        img: "rewind",
        onClick: () => this.rewind(),
      }),

      CommandButton({
        className: "primary",
        active: !paused || seeking,
        img: "pause",
        onClick: () => this.pause(),
      }),

      CommandButton({
        className: "",
        active: paused,
        img: "resume",
        onClick: () => this.resume(),
      }),
    ];
  }

  updateOverlayWidth() {
    const el = ReactDOM.findDOMNode(this).querySelector(".progressBar");
    return el ? el.clientWidth : 1;
  }

  // calculate pixel distance from two points
  getPixelDistance(to, from) {
    const toPos = this.getVisiblePosition(to);
    const fromPos = this.getVisiblePosition(from);

    return (toPos - fromPos) * this.overlayWidth;
  }

  // Get the position of an execution point on the visible part of the timeline,
  // in the range [0, 1].
  getVisiblePosition(executionPoint) {
    const { start, end } = this.state;

    if (!executionPoint) {
      return 0;
    }

    const time = executionPointTime(executionPoint);
    const position = time / recordingEndTime();

    if (position < start) {
      return 0;
    }

    if (position > end) {
      return 1;
    }

    return (position - start) / (end - start);
  }

  // Get the pixel offset for an execution point.
  getPixelOffset(point) {
    return this.getVisiblePosition(point) * this.overlayWidth;
  }

  renderMessage(message, index) {
    const {
      messages,
      executionPoint,
      pausedMessage,
      highlightedMessage,
    } = this.state;

    const offset = this.getPixelOffset(message.executionPoint);
    const previousMessage = messages[index - 1];

    if (offset < 0) {
      return null;
    }

    // Check to see if two messages overlay each other on the timeline
    const isOverlayed =
      previousMessage &&
      this.getPixelDistance(
        message.executionPoint,
        previousMessage.executionPoint
      ) < markerWidth;

    // Check to see if a message appears after the current execution point
    const isFuture =
      this.getPixelDistance(message.executionPoint, executionPoint) >
      markerWidth / 2;

    const isHighlighted = highlightedMessage == message.id;

    const uncached = message.executionPoint && !this.isCached(message);

    const atPausedLocation =
      pausedMessage && sameLocation(pausedMessage, message);

    let frameLocation = "";
    if (message.frame) {
      const { source, line, column } = message.frame;
      const filename = source.split("/").pop();
      frameLocation = `${filename}:${line}`;
      if (column > 100) {
        frameLocation += `:${column}`;
      }
    }

    return dom.a({
      className: classname("message", {
        overlayed: isOverlayed,
        future: isFuture,
        highlighted: isHighlighted,
        uncached,
        location: atPausedLocation,
      }),
      style: {
        left: `${Math.max(offset - markerWidth / 2, 0)}px`,
        zIndex: `${index + 100}`,
      },
      title: uncached
        ? "Loading..."
        : getFormatStr("jumpMessage2", frameLocation),
      onClick: e => {
        e.preventDefault();
        e.stopPropagation();
        this.seek(message.executionPoint);
      },
      onMouseEnter: () => this.onMessageMouseEnter(message, offset),
      onMouseLeave: () => this.onMessageMouseLeave(),
    });
  }

  renderMessages() {
    const messages = this.state.messages;
    return messages.map((message, index) => this.renderMessage(message, index));
  }

  renderHoverPoint() {
    const { hoverPoint } = this.state;
    if (!hoverPoint) {
      return [];
    }
    const offset = this.getPixelOffset(hoverPoint);
    return [dom.span({
      className: classname("hoverPoint"),
      style: {
        left: `${Math.max(offset - markerWidth / 2, 0)}px`,
        zIndex: 1000,
      },
    })];
  }

  renderTicks() {
    const tickSize = this.getTickSize();
    const ticks = Math.round(this.overlayWidth / tickSize);
    return range(ticks).map((value, index) => this.renderTick(index));
  }

  renderTick(index) {
    const { executionPoint, hoveredMessageOffset } = this.state;
    const tickSize = this.getTickSize();
    const offset = Math.round(this.getPixelOffset(executionPoint));
    const position = index * tickSize;
    const isFuture = position > offset;
    const shouldHighlight = hoveredMessageOffset > position;

    return dom.span({
      className: classname("tick", {
        future: isFuture,
        highlight: shouldHighlight,
      }),
      style: {
        left: `${position}px`,
        width: `${tickSize}px`,
      },
    });
  }

  renderUnscannedRegions() {
    return this.state.unscannedRegions.map(
      this.renderUnscannedRegion.bind(this)
    );
  }

  renderUnscannedRegion({ start, end, traversed }) {
    let startOffset = this.getPixelOffset(start);
    let endOffset = this.getPixelOffset(end);

    if (startOffset >= this.overlayWidth || endOffset <= 0) {
      return null;
    }

    if (startOffset < 0) {
      startOffset = 0;
    }

    if (endOffset > this.overlayWidth) {
      endOffset = this.overlayWidth;
    }

    return dom.span({
      className: traversed ? classname("unscanned") : classname("untraversed"),
      style: {
        left: `${startOffset}px`,
        width: `${endOffset - startOffset}px`,
      },
    });
  }

  render() {
    const percent = this.getVisiblePosition(this.state.executionPoint) * 100;

    const recording = this.isRecording();
    const { shouldAnimate } = this.state;
    return div(
      {
        className: "webreplay-player",
      },
      div(
        {
          id: "overlay",
          className: classname("", {
            recording: recording,
            paused: !recording,
          }),
        },
        div(
          {
            className: classname("overlay-container", {
              animate: shouldAnimate,
            }),
          },
          div({ className: "commands" }, ...this.renderCommands()),
          div(
            {
              className: "progressBar",
              onClick: this.onProgressBarClick,
              onDoubleClick: () => this.setState({ start: 0, end: 1 }),
              onMouseOver: this.onProgressBarMouseOver,
              onMouseLeave: this.onPlayerMouseLeave,
            },
            div({
              className: "progress",
              style: { width: `${percent}%` },
            }),
            div({
              className: "progress-line",
              style: { width: `${percent}%` },
            }),
            div({
              className: "progress-line end",
              style: { left: `${percent}%`, width: `${100 - percent}%` },
            }),
            ...this.renderMessages(),
            ...this.renderHoverPoint(),
            ...this.renderTicks(),
            ...this.renderUnscannedRegions()
          )
        )
      )
    );
  }
}

module.exports = WebReplayPlayer;
