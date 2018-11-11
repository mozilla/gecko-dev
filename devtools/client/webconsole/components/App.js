/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");

const actions = require("devtools/client/webconsole/actions/index");
const ConsoleOutput = createFactory(require("devtools/client/webconsole/components/ConsoleOutput"));
const FilterBar = createFactory(require("devtools/client/webconsole/components/FilterBar"));
const SideBar = createFactory(require("devtools/client/webconsole/components/SideBar"));
const ReverseSearchInput = createFactory(require("devtools/client/webconsole/components/ReverseSearchInput"));
const JSTerm = createFactory(require("devtools/client/webconsole/components/JSTerm"));
const NotificationBox = createFactory(require("devtools/client/shared/components/NotificationBox").NotificationBox);

const l10n = require("devtools/client/webconsole/webconsole-l10n");
const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");

const SELF_XSS_OK = l10n.getStr("selfxss.okstring");
const SELF_XSS_MSG = l10n.getFormatStr("selfxss.msg", [SELF_XSS_OK]);

const {
  getNotificationWithValue,
  PriorityLevels,
} = require("devtools/client/shared/components/NotificationBox");

const { getAllNotifications } = require("devtools/client/webconsole/selectors/notifications");
const { div } = dom;
const isMacOS = Services.appinfo.OS === "Darwin";

/**
 * Console root Application component.
 */
class App extends Component {
  static get propTypes() {
    return {
      attachRefToHud: PropTypes.func.isRequired,
      dispatch: PropTypes.func.isRequired,
      hud: PropTypes.object.isRequired,
      notifications: PropTypes.object,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      closeSplitConsole: PropTypes.func.isRequired,
      jstermCodeMirror: PropTypes.bool,
      jstermReverseSearch: PropTypes.bool,
      currentReverseSearchEntry: PropTypes.string,
      reverseSearchInputVisible: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.onClick = this.onClick.bind(this);
    this.onPaste = this.onPaste.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  onKeyDown(event) {
    const {
      dispatch,
      jstermReverseSearch,
    } = this.props;

    if (
      jstermReverseSearch && (
        (!isMacOS && event.key === "F9") ||
        (isMacOS && event.key === "r" && event.ctrlKey === true)
      )
    ) {
      dispatch(actions.reverseSearchInputToggle());
      event.stopPropagation();
    }
  }

  onClick(event) {
    const target = event.originalTarget || event.target;
    const {
      reverseSearchInputVisible,
      dispatch,
      hud,
    } = this.props;

    if (reverseSearchInputVisible === true && !target.closest(".reverse-search")) {
      event.preventDefault();
      event.stopPropagation();
      dispatch(actions.reverseSearchInputToggle());
      return;
    }

    // Do not focus on middle/right-click or 2+ clicks.
    if (event.detail !== 1 || event.button !== 0) {
      return;
    }

    // Do not focus if a link was clicked
    if (target.closest("a")) {
      return;
    }

    // Do not focus if an input field was clicked
    if (target.closest("input")) {
      return;
    }

    // Do not focus if the click happened in the reverse search toolbar.
    if (target.closest(".reverse-search")) {
      return;
    }

    // Do not focus if something other than the output region was clicked
    // (including e.g. the clear messages button in toolbar)
    if (!target.closest(".webconsole-output-wrapper")) {
      return;
    }

    // Do not focus if something is selected
    const selection = hud.document.defaultView.getSelection();
    if (selection && !selection.isCollapsed) {
      return;
    }

    if (hud && hud.jsterm) {
      hud.jsterm.focus();
    }
  }

  onPaste(event) {
    const {
      dispatch,
      hud,
      notifications,
    } = this.props;

    const {
      usageCount,
      CONSOLE_ENTRY_THRESHOLD,
    } = WebConsoleUtils;

    // Bail out if self-xss notification is suppressed.
    if (hud.isBrowserConsole || usageCount >= CONSOLE_ENTRY_THRESHOLD) {
      return;
    }

    // Stop event propagation, so the clipboard content is *not* inserted.
    event.preventDefault();
    event.stopPropagation();

    // Bail out if self-xss notification is already there.
    if (getNotificationWithValue(notifications, "selfxss-notification")) {
      return;
    }

    const input = event.target;

    // Cleanup function if notification is closed by the user.
    const removeCallback = (eventType) => {
      if (eventType == "removed") {
        input.removeEventListener("keyup", pasteKeyUpHandler);
        dispatch(actions.removeNotification("selfxss-notification"));
      }
    };

    // Create self-xss notification
    dispatch(actions.appendNotification(
      SELF_XSS_MSG,
      "selfxss-notification",
      null,
      PriorityLevels.PRIORITY_WARNING_HIGH,
      null,
      removeCallback
    ));

    // Remove notification automatically when the user types "allow pasting".
    const pasteKeyUpHandler = (e) => {
      const value = e.target.value;
      if (value.includes(SELF_XSS_OK)) {
        dispatch(actions.removeNotification("selfxss-notification"));
        input.removeEventListener("keyup", pasteKeyUpHandler);
        WebConsoleUtils.usageCount = WebConsoleUtils.CONSOLE_ENTRY_THRESHOLD;
      }
    };

    input.addEventListener("keyup", pasteKeyUpHandler);
  }

  // Rendering

  render() {
    const {
      attachRefToHud,
      hud,
      notifications,
      onFirstMeaningfulPaint,
      serviceContainer,
      closeSplitConsole,
      jstermCodeMirror,
      jstermReverseSearch,
    } = this.props;

    const classNames = ["webconsole-output-wrapper"];
    if (jstermCodeMirror) {
      classNames.push("jsterm-cm");
    }

    // Render the entire Console panel. The panel consists
    // from the following parts:
    // * FilterBar - Buttons & free text for content filtering
    // * Content - List of logs & messages
    // * NotificationBox - Notifications for JSTerm (self-xss warning at the moment)
    // * JSTerm - Input command line.
    // * ReverseSearchInput - Reverse search input.
    // * SideBar - Object inspector
    return (
      div({
        className: classNames.join(" "),
        onKeyDown: this.onKeyDown,
        onClick: this.onClick,
        ref: node => {
          this.node = node;
        }},
        div({className: "webconsole-flex-wrapper"},
          FilterBar({
            hidePersistLogsCheckbox: hud.isBrowserConsole,
            serviceContainer: {
              attachRefToHud,
            },
            closeSplitConsole,
          }),
          ConsoleOutput({
            serviceContainer,
            onFirstMeaningfulPaint,
          }),
          NotificationBox({
            id: "webconsole-notificationbox",
            notifications,
          }),
          JSTerm({
            hud,
            serviceContainer,
            onPaste: this.onPaste,
            codeMirrorEnabled: jstermCodeMirror,
          }),
          jstermReverseSearch
            ? ReverseSearchInput({
              hud,
            })
            : null
        ),
        SideBar({
          serviceContainer,
        }),
      )
    );
  }
}

const mapStateToProps = state => ({
  notifications: getAllNotifications(state),
  reverseSearchInputVisible: state.ui.reverseSearchInputVisible,
});

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(App);
