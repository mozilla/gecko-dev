/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const { Component } = require("devtools/client/shared/vendor/react");
const { createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const actions = require("devtools/client/webconsole/actions/index");
const { l10n } = require("devtools/client/webconsole/utils/messages");

// Additional Components
const MenuButton = createFactory(
  require("devtools/client/shared/components/menu/MenuButton")
);
const MenuItem = createFactory(
  require("devtools/client/shared/components/menu/MenuItem")
);
const MenuList = createFactory(
  require("devtools/client/shared/components/menu/MenuList")
);

class ConsoleSettings extends Component {
  static get propTypes() {
    return {
      compactToolbar: PropTypes.bool.isRequired,
      dispatch: PropTypes.func.isRequired,
      groupWarnings: PropTypes.bool.isRequired,
      hideCompactToolbarCheckbox: PropTypes.bool.isRequired,
      hidePersistLogsCheckbox: PropTypes.bool.isRequired,
      hideShowContentMessagesCheckbox: PropTypes.bool.isRequired,
      persistLogs: PropTypes.bool.isRequired,
      showContentMessages: PropTypes.bool.isRequired,
      timestampsVisible: PropTypes.bool.isRequired,
      webConsoleUI: PropTypes.object.isRequired,
    };
  }

  renderMenuItems() {
    const {
      dispatch,
      groupWarnings,
      hidePersistLogsCheckbox,
      hideShowContentMessagesCheckbox,
      persistLogs,
      showContentMessages,
      timestampsVisible,
    } = this.props;

    const items = [];

    // Persist Logs
    if (!hidePersistLogsCheckbox) {
      items.push(
        MenuItem({
          key: "webconsole-console-settings-menu-item-persistent-logs",
          checked: persistLogs,
          className:
            "menu-item webconsole-console-settings-menu-item-persistentLogs",
          label: l10n.getStr(
            "webconsole.console.settings.menu.item.enablePersistentLogs.label"
          ),
          tooltip: l10n.getStr(
            "webconsole.console.settings.menu.item.enablePersistentLogs.tooltip"
          ),
          onClick: () => dispatch(actions.persistToggle()),
        })
      );
    }

    // Show Content Messages
    if (!hideShowContentMessagesCheckbox) {
      items.push(
        MenuItem({
          key: "webconsole-console-settings-menu-item-content-messages",
          checked: showContentMessages,
          className:
            "menu-item webconsole-console-settings-menu-item-contentMessages",
          label: l10n.getStr("browserconsole.contentMessagesCheckbox.label"),
          tooltip: l10n.getStr(
            "browserconsole.contentMessagesCheckbox.tooltip"
          ),
          onClick: () => dispatch(actions.contentMessagesToggle()),
        })
      );
    }

    // Timestamps
    items.push(
      MenuItem({
        key: "webconsole-console-settings-menu-item-timestamps",
        checked: timestampsVisible,
        className: "menu-item webconsole-console-settings-menu-item-timestamps",
        label: l10n.getStr(
          "webconsole.console.settings.menu.item.timestamps.label"
        ),
        tooltip: l10n.getStr(
          "webconsole.console.settings.menu.item.timestamps.tooltip"
        ),
        onClick: () => dispatch(actions.timestampsToggle()),
      })
    );

    // Warning Groups
    items.push(
      MenuItem({
        key: "webconsole-console-settings-menu-item-warning-groups",
        checked: groupWarnings,
        className:
          "menu-item webconsole-console-settings-menu-item-warning-groups",
        label: l10n.getStr(
          "webconsole.console.settings.menu.item.warningGroups.label"
        ),
        tooltip: l10n.getStr(
          "webconsole.console.settings.menu.item.warningGroups.tooltip"
        ),
        onClick: () => dispatch(actions.warningGroupsToggle()),
      })
    );

    return MenuList({ id: "webconsole-console-settings-menu-list" }, items);
  }

  render() {
    const { webConsoleUI } = this.props;
    const doc = webConsoleUI.document;
    const toolbox = webConsoleUI.wrapper.toolbox;

    return MenuButton(
      {
        menuId: "webconsole-console-settings-menu-button",
        toolboxDoc: toolbox ? toolbox.doc : doc,
        className: "devtools-button webconsole-console-settings-menu-button",
        title: l10n.getStr("webconsole.console.settings.menu.button.tooltip"),
      },
      this.renderMenuItems()
    );
  }
}

module.exports = ConsoleSettings;
