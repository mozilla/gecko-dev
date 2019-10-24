/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Services = require("Services");
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");

const actions = require("devtools/client/webconsole/actions/index");
const {
  FILTERBAR_DISPLAY_MODES,
} = require("devtools/client/webconsole/constants");
const ConsoleOutput = createFactory(
  require("devtools/client/webconsole/components/Output/ConsoleOutput")
);
const FilterBar = createFactory(
  require("devtools/client/webconsole/components/FilterBar/FilterBar")
);
const SideBar = createFactory(
  require("devtools/client/webconsole/components/SideBar")
);
const ReverseSearchInput = createFactory(
  require("devtools/client/webconsole/components/Input/ReverseSearchInput")
);
const EditorToolbar = createFactory(
  require("devtools/client/webconsole/components/Input/EditorToolbar")
);
const JSTerm = createFactory(
  require("devtools/client/webconsole/components/Input/JSTerm")
);
const ConfirmDialog = createFactory(
  require("devtools/client/webconsole/components/Input/ConfirmDialog")
);
const NotificationBox = createFactory(
  require("devtools/client/shared/components/NotificationBox").NotificationBox
);
const GridElementWidthResizer = createFactory(
  require("devtools/client/shared/components/splitter/GridElementWidthResizer")
);

const l10n = require("devtools/client/webconsole/utils/l10n");
const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");

const SELF_XSS_OK = l10n.getStr("selfxss.okstring");
const SELF_XSS_MSG = l10n.getFormatStr("selfxss.msg", [SELF_XSS_OK]);

const {
  getNotificationWithValue,
  PriorityLevels,
} = require("devtools/client/shared/components/NotificationBox");

const {
  getAllNotifications,
} = require("devtools/client/webconsole/selectors/notifications");
const { div } = dom;
const isMacOS = Services.appinfo.OS === "Darwin";

/**
 * Console root Application component.
 */
class App extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      webConsoleUI: PropTypes.object.isRequired,
      notifications: PropTypes.object,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      closeSplitConsole: PropTypes.func.isRequired,
      autocomplete: PropTypes.bool,
      currentReverseSearchEntry: PropTypes.string,
      reverseSearchInputVisible: PropTypes.bool,
      reverseSearchInitialValue: PropTypes.string,
      editorMode: PropTypes.bool,
      editorWidth: PropTypes.number,
      hidePersistLogsCheckbox: PropTypes.bool,
      hideShowContentMessagesCheckbox: PropTypes.bool,
      sidebarVisible: PropTypes.bool.isRequired,
      filterBarDisplayMode: PropTypes.oneOf([
        ...Object.values(FILTERBAR_DISPLAY_MODES),
      ]).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onClick = this.onClick.bind(this);
    this.onPaste = this.onPaste.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onBlur = this.onBlur.bind(this);
  }

  componentDidMount() {
    window.addEventListener("blur", this.onBlur);
  }

  onBlur() {
    this.props.dispatch(actions.autocompleteClear());
  }

  onKeyDown(event) {
    const { dispatch, webConsoleUI } = this.props;

    if (
      (!isMacOS && event.key === "F9") ||
      (isMacOS && event.key === "r" && event.ctrlKey === true)
    ) {
      const initialValue =
        webConsoleUI.jsterm && webConsoleUI.jsterm.getSelectedText();
      dispatch(actions.reverseSearchInputToggle({ initialValue }));
      event.stopPropagation();
    }

    if (
      event.key.toLowerCase() === "b" &&
      ((isMacOS && event.metaKey) || (!isMacOS && event.ctrlKey))
    ) {
      event.stopPropagation();
      event.preventDefault();
      dispatch(actions.editorToggle());
    }
  }

  onClick(event) {
    const target = event.originalTarget || event.target;
    const { reverseSearchInputVisible, dispatch, webConsoleUI } = this.props;

    if (
      reverseSearchInputVisible === true &&
      !target.closest(".reverse-search")
    ) {
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
    if (!target.closest(".webconsole-app")) {
      return;
    }

    // Do not focus if something is selected
    const selection = webConsoleUI.document.defaultView.getSelection();
    if (selection && !selection.isCollapsed) {
      return;
    }

    if (webConsoleUI && webConsoleUI.jsterm) {
      webConsoleUI.jsterm.focus();
    }
  }

  onPaste(event) {
    const { dispatch, webConsoleUI, notifications } = this.props;

    const { usageCount, CONSOLE_ENTRY_THRESHOLD } = WebConsoleUtils;

    // Bail out if self-xss notification is suppressed.
    if (
      webConsoleUI.isBrowserConsole ||
      usageCount >= CONSOLE_ENTRY_THRESHOLD
    ) {
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
    const removeCallback = eventType => {
      if (eventType == "removed") {
        input.removeEventListener("keyup", pasteKeyUpHandler);
        dispatch(actions.removeNotification("selfxss-notification"));
      }
    };

    // Create self-xss notification
    dispatch(
      actions.appendNotification(
        SELF_XSS_MSG,
        "selfxss-notification",
        null,
        PriorityLevels.PRIORITY_WARNING_HIGH,
        null,
        removeCallback
      )
    );

    // Remove notification automatically when the user types "allow pasting".
    const pasteKeyUpHandler = e => {
      const value = e.target.value;
      if (value.includes(SELF_XSS_OK)) {
        dispatch(actions.removeNotification("selfxss-notification"));
        input.removeEventListener("keyup", pasteKeyUpHandler);
        WebConsoleUtils.usageCount = WebConsoleUtils.CONSOLE_ENTRY_THRESHOLD;
      }
    };

    input.addEventListener("keyup", pasteKeyUpHandler);
  }

  renderFilterBar() {
    const {
      closeSplitConsole,
      filterBarDisplayMode,
      hidePersistLogsCheckbox,
      hideShowContentMessagesCheckbox,
      webConsoleUI,
    } = this.props;

    return FilterBar({
      key: "filterbar",
      hidePersistLogsCheckbox,
      hideShowContentMessagesCheckbox,
      closeSplitConsole,
      displayMode: filterBarDisplayMode,
      webConsoleUI,
    });
  }

  renderEditorToolbar() {
    const {
      editorMode,
      dispatch,
      reverseSearchInputVisible,
      serviceContainer,
      webConsoleUI,
    } = this.props;

    return editorMode
      ? EditorToolbar({
          key: "editor-toolbar",
          editorMode,
          dispatch,
          reverseSearchInputVisible,
          serviceContainer,
          webConsoleUI,
        })
      : null;
  }

  renderConsoleOutput() {
    const { onFirstMeaningfulPaint, serviceContainer } = this.props;

    return ConsoleOutput({
      key: "console-output",
      serviceContainer,
      onFirstMeaningfulPaint,
    });
  }

  renderJsTerm() {
    const {
      webConsoleUI,
      serviceContainer,
      autocomplete,
      editorMode,
      editorWidth,
    } = this.props;

    return JSTerm({
      key: "jsterm",
      webConsoleUI,
      serviceContainer,
      onPaste: this.onPaste,
      autocomplete,
      editorMode,
      editorWidth,
    });
  }

  renderReverseSearch() {
    const { serviceContainer, reverseSearchInitialValue } = this.props;

    return ReverseSearchInput({
      key: "reverse-search-input",
      setInputValue: serviceContainer.setInputValue,
      focusInput: serviceContainer.focusInput,
      initialValue: reverseSearchInitialValue,
    });
  }

  renderSideBar() {
    const { serviceContainer, sidebarVisible } = this.props;
    return SideBar({
      key: "sidebar",
      serviceContainer,
      visible: sidebarVisible,
    });
  }

  renderNotificationBox() {
    const { notifications, editorMode } = this.props;

    return NotificationBox({
      id: "webconsole-notificationbox",
      key: "notification-box",
      displayBorderTop: !editorMode,
      displayBorderBottom: editorMode,
      wrapping: true,
      notifications,
    });
  }

  renderConfirmDialog() {
    const { webConsoleUI, serviceContainer } = this.props;

    return ConfirmDialog({
      webConsoleUI,
      serviceContainer,
      key: "confirm-dialog",
    });
  }

  renderRootElement(children) {
    const { editorMode, serviceContainer } = this.props;

    const classNames = ["webconsole-app"];
    if (editorMode) {
      classNames.push("jsterm-editor");
    }
    if (serviceContainer.canRewind()) {
      classNames.push("can-rewind");
    }

    return div(
      {
        className: classNames.join(" "),
        onKeyDown: this.onKeyDown,
        onClick: this.onClick,
        ref: node => {
          this.node = node;
        },
      },
      children
    );
  }

  render() {
    const { webConsoleUI, editorMode, dispatch } = this.props;

    const filterBar = this.renderFilterBar();
    const editorToolbar = this.renderEditorToolbar();
    const consoleOutput = this.renderConsoleOutput();
    const notificationBox = this.renderNotificationBox();
    const jsterm = this.renderJsTerm();
    const reverseSearch = this.renderReverseSearch();
    const sidebar = this.renderSideBar();
    const confirmDialog = this.renderConfirmDialog();

    return this.renderRootElement([
      filterBar,
      editorToolbar,
      dom.div(
        { className: "flexible-output-input", key: "in-out-container" },
        consoleOutput,
        notificationBox,
        jsterm
      ),
      GridElementWidthResizer({
        key: "editor-resizer",
        enabled: editorMode,
        position: "end",
        className: "editor-resizer",
        getControlledElementNode: () => webConsoleUI.jsterm.node,
        onResizeEnd: width => dispatch(actions.setEditorWidth(width)),
      }),
      reverseSearch,
      sidebar,
      confirmDialog,
    ]);
  }
}

const mapStateToProps = state => ({
  notifications: getAllNotifications(state),
  reverseSearchInputVisible: state.ui.reverseSearchInputVisible,
  reverseSearchInitialValue: state.ui.reverseSearchInitialValue,
  editorMode: state.ui.editor,
  editorWidth: state.ui.editorWidth,
  sidebarVisible: state.ui.sidebarVisible,
  filterBarDisplayMode: state.ui.filterBarDisplayMode,
});

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(
  mapStateToProps,
  mapDispatchToProps
)(App);
