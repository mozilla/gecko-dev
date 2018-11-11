/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { l10n } =
  require("devtools/client/webconsole/utils/messages");
const actions =
  require("devtools/client/webconsole/actions/index");
const { MESSAGE_SOURCE, MESSAGE_TYPE } =
  require("devtools/client/webconsole/constants");
const CollapseButton =
  require("devtools/client/webconsole/components/CollapseButton");
const { MessageIndent } =
  require("devtools/client/webconsole/components/MessageIndent");
const MessageIcon =
  require("devtools/client/webconsole/components/MessageIcon");
const MessageRepeat =
  require("devtools/client/webconsole/components/MessageRepeat");
const FrameView = createFactory(require("devtools/client/shared/components/Frame"));
const StackTrace = createFactory(require("devtools/client/shared/components/StackTrace"));

class Message extends Component {
  static get propTypes() {
    return {
      open: PropTypes.bool,
      collapsible: PropTypes.bool,
      collapseTitle: PropTypes.string,
      source: PropTypes.string.isRequired,
      type: PropTypes.string.isRequired,
      level: PropTypes.string.isRequired,
      indent: PropTypes.number.isRequired,
      topLevelClasses: PropTypes.array.isRequired,
      messageBody: PropTypes.any.isRequired,
      repeat: PropTypes.any,
      frame: PropTypes.any,
      attachment: PropTypes.any,
      stacktrace: PropTypes.any,
      messageId: PropTypes.string,
      executionPoint: PropTypes.string,
      scrollToMessage: PropTypes.bool,
      exceptionDocURL: PropTypes.string,
      request: PropTypes.object,
      dispatch: PropTypes.func,
      timeStamp: PropTypes.number,
      timestampsVisible: PropTypes.bool.isRequired,
      serviceContainer: PropTypes.shape({
        emitNewMessage: PropTypes.func.isRequired,
        onViewSourceInDebugger: PropTypes.func,
        onViewSourceInScratchpad: PropTypes.func,
        onViewSourceInStyleEditor: PropTypes.func,
        openContextMenu: PropTypes.func.isRequired,
        openLink: PropTypes.func.isRequired,
        sourceMapService: PropTypes.any,
      }),
      notes: PropTypes.arrayOf(PropTypes.shape({
        messageBody: PropTypes.string.isRequired,
        frame: PropTypes.any,
      })),
      isPaused: PropTypes.bool,
    };
  }

  static get defaultProps() {
    return {
      indent: 0,
    };
  }

  constructor(props) {
    super(props);
    this.onLearnMoreClick = this.onLearnMoreClick.bind(this);
    this.toggleMessage = this.toggleMessage.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.renderIcon = this.renderIcon.bind(this);
  }

  componentDidMount() {
    if (this.messageNode) {
      if (this.props.scrollToMessage) {
        this.messageNode.scrollIntoView();
      }
      // Event used in tests. Some message types don't pass it in because existing tests
      // did not emit for them.
      if (this.props.serviceContainer) {
        this.props.serviceContainer.emitNewMessage(
          this.messageNode, this.props.messageId, this.props.timeStamp);
      }
    }
  }

  onLearnMoreClick(e) {
    const {exceptionDocURL} = this.props;
    this.props.serviceContainer.openLink(exceptionDocURL, e);
  }

  toggleMessage(e) {
    const { open, dispatch, messageId } = this.props;
    if (open) {
      dispatch(actions.messageClose(messageId));
    } else {
      dispatch(actions.messageOpen(messageId));
    }
  }

  onContextMenu(e) {
    const { serviceContainer, source, request, messageId } = this.props;
    const messageInfo = {
      source,
      request,
      messageId,
    };
    serviceContainer.openContextMenu(e, messageInfo);
    e.stopPropagation();
    e.preventDefault();
  }

  renderIcon() {
    const { level, messageId, executionPoint, serviceContainer } = this.props;

    return MessageIcon({
      level,
      onRewindClick: serviceContainer.canRewind()
        ? () => serviceContainer.jumpToExecutionPoint(executionPoint, messageId)
        : null,
    });
  }

  render() {
    const {
      open,
      collapsible,
      collapseTitle,
      source,
      type,
      isPaused,
      level,
      indent,
      topLevelClasses,
      messageBody,
      frame,
      stacktrace,
      serviceContainer,
      exceptionDocURL,
      timeStamp = Date.now(),
      timestampsVisible,
      notes,
    } = this.props;

    topLevelClasses.push("message", source, type, level, isPaused ? "paused" : "");
    if (open) {
      topLevelClasses.push("open");
    }

    let timestampEl;
    if (timestampsVisible === true) {
      timestampEl = dom.span({
        className: "timestamp devtools-monospace",
      }, l10n.timestampString(timeStamp));
    }

    const icon = this.renderIcon();

    // Figure out if there is an expandable part to the message.
    let attachment = null;
    if (this.props.attachment) {
      attachment = this.props.attachment;
    } else if (stacktrace && open) {
      attachment = dom.div(
        {
          className: "stacktrace devtools-monospace",
        },
        StackTrace({
          stacktrace: stacktrace,
          onViewSourceInDebugger: serviceContainer.onViewSourceInDebugger
            || serviceContainer.onViewSource,
          onViewSourceInScratchpad: serviceContainer.onViewSourceInScratchpad
            || serviceContainer.onViewSource,
          sourceMapService: serviceContainer.sourceMapService,
        })
      );
    }

    // If there is an expandable part, make it collapsible.
    let collapse = null;
    if (collapsible) {
      collapse = CollapseButton({
        open,
        title: collapseTitle,
        onClick: this.toggleMessage,
      });
    }

    let notesNodes;
    if (notes) {
      notesNodes = notes.map(note => dom.span(
        { className: "message-flex-body error-note" },
        dom.span({ className: "message-body devtools-monospace" },
          "note: " + note.messageBody
        ),
        dom.span({ className: "message-location devtools-monospace" },
          note.frame ? FrameView({
            frame: note.frame,
            onClick: serviceContainer
              ? serviceContainer.onViewSourceInDebugger || serviceContainer.onViewSource
              : undefined,
            showEmptyPathAsHost: true,
            sourceMapService: serviceContainer
              ? serviceContainer.sourceMapService
              : undefined,
          }) : null
        )));
    } else {
      notesNodes = [];
    }

    const repeat = this.props.repeat && this.props.repeat > 1 ?
      MessageRepeat({repeat: this.props.repeat}) : null;

    let onFrameClick;
    if (serviceContainer && frame) {
      if (source === MESSAGE_SOURCE.CSS) {
        onFrameClick = serviceContainer.onViewSourceInStyleEditor
          || serviceContainer.onViewSource;
      } else if (/^Scratchpad\/\d+$/.test(frame.source)) {
        onFrameClick = serviceContainer.onViewSourceInScratchpad
          || serviceContainer.onViewSource;
      } else {
        // Point everything else to debugger, if source not available,
        // it will fall back to view-source.
        onFrameClick = serviceContainer.onViewSourceInDebugger
          || serviceContainer.onViewSource;
      }
    }

    // Configure the location.
    const location = dom.span({ className: "message-location devtools-monospace" },
      frame ? FrameView({
        frame,
        onClick: onFrameClick,
        showEmptyPathAsHost: true,
        sourceMapService:
          serviceContainer ? serviceContainer.sourceMapService : undefined,
      }) : null
    );

    let learnMore;
    if (exceptionDocURL) {
      learnMore = dom.a({
        className: "learn-more-link webconsole-learn-more-link",
        title: exceptionDocURL.split("?")[0],
        onClick: this.onLearnMoreClick,
      }, `[${l10n.getStr("webConsoleMoreInfoLabel")}]`);
    }

    const bodyElements = Array.isArray(messageBody) ? messageBody : [messageBody];

    return dom.div({
      className: topLevelClasses.join(" "),
      onContextMenu: this.onContextMenu,
      ref: node => {
        this.messageNode = node;
      },
      "aria-live": type === MESSAGE_TYPE.COMMAND ? "off" : "polite",
    },
      timestampEl,
      MessageIndent({indent}),
      icon,
      collapse,
      dom.span({ className: "message-body-wrapper" },
        dom.span({
          className: "message-flex-body",
          onClick: collapsible ? this.toggleMessage : undefined,
        },
          // Add whitespaces for formatting when copying to the clipboard.
          timestampEl ? " " : null,
          dom.span({ className: "message-body devtools-monospace" },
            ...bodyElements,
            learnMore
          ),
          repeat ? " " : null,
          repeat,
          " ", location
        ),
        // Add a newline for formatting when copying to the clipboard.
        "\n",
        // If an attachment is displayed, the final newline is handled by the attachment.
        attachment,
        ...notesNodes
      )
    );
  }
}

module.exports = Message;
