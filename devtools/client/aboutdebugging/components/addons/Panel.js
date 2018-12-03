/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonManager } = require("resource://gre/modules/AddonManager.jsm");
const { Management } = require("resource://gre/modules/Extension.jsm");
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

const AddonsControls = createFactory(require("./Controls"));
const AddonTarget = createFactory(require("./Target"));
const PanelHeader = createFactory(require("../PanelHeader"));
const TargetList = createFactory(require("../TargetList"));

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const ExtensionIcon = "chrome://mozapps/skin/extensions/extensionGeneric.svg";
const CHROME_ENABLED_PREF = "devtools.chrome.enabled";
const REMOTE_ENABLED_PREF = "devtools.debugger.remote-enabled";
const SYSTEM_ENABLED_PREF = "devtools.aboutdebugging.showSystemAddons";
const WEB_EXT_URL = "https://developer.mozilla.org/Add-ons" +
                    "/WebExtensions/Getting_started_with_web-ext";

class AddonsPanel extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      connect: PropTypes.object,
      id: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      extensions: [],
      debugDisabled: false,
      showSystemAddons: false,
    };

    this.updateDebugStatus = this.updateDebugStatus.bind(this);
    this.updateShowSystemStatus = this.updateShowSystemStatus.bind(this);
    this.updateAddonsList = this.updateAddonsList.bind(this);
    this.onInstalled = this.onInstalled.bind(this);
    this.onUninstalled = this.onUninstalled.bind(this);
    this.onEnabled = this.onEnabled.bind(this);
    this.onDisabled = this.onDisabled.bind(this);
  }

  componentDidMount() {
    AddonManager.addAddonListener(this);
    // Listen to startup since that's when errors and warnings
    // get populated on the extension.
    Management.on("startup", this.updateAddonsList);

    Services.prefs.addObserver(CHROME_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.addObserver(REMOTE_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.addObserver(SYSTEM_ENABLED_PREF,
      this.updateShowSystemStatus);

    this.updateDebugStatus();
    this.updateShowSystemStatus();
    this.updateAddonsList();
  }

  componentWillUnmount() {
    AddonManager.removeAddonListener(this);
    Management.off("startup", this.updateAddonsList);

    Services.prefs.removeObserver(CHROME_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.removeObserver(REMOTE_ENABLED_PREF,
      this.updateDebugStatus);
    Services.prefs.removeObserver(SYSTEM_ENABLED_PREF,
      this.updateShowSystemStatus);
  }

  updateDebugStatus() {
    const debugDisabled =
      !Services.prefs.getBoolPref(CHROME_ENABLED_PREF) ||
      !Services.prefs.getBoolPref(REMOTE_ENABLED_PREF);

    this.setState({ debugDisabled });
  }

  updateShowSystemStatus() {
    const showSystemAddons = Services.prefs.getBoolPref(SYSTEM_ENABLED_PREF, false);
    this.setState({ showSystemAddons });
  }

  updateAddonsList() {
    this.props.client.mainRoot.listAddons()
      .then(addons => {
        const extensions = addons.filter(addon => addon.debuggable).map(addon => {
          return {
            addonTargetFront: addon,
            addonID: addon.id,
            icon: addon.iconURL || ExtensionIcon,
            isSystem: addon.isSystem,
            manifestURL: addon.manifestURL,
            name: addon.name,
            temporarilyInstalled: addon.temporarilyInstalled,
            url: addon.url,
            warnings: addon.warnings,
          };
        });

        this.setState({ extensions });

        const { AboutDebugging } = window;
        AboutDebugging.emit("addons-updated");
      }, error => {
        throw new Error("Client error while listing addons: " + error);
      });
  }

  /**
   * Mandatory callback as AddonManager listener.
   */
  onInstalled() {
    this.updateAddonsList();
  }

  /**
   * Mandatory callback as AddonManager listener.
   */
  onUninstalled() {
    this.updateAddonsList();
  }

  /**
   * Mandatory callback as AddonManager listener.
   */
  onEnabled() {
    this.updateAddonsList();
  }

  /**
   * Mandatory callback as AddonManager listener.
   */
  onDisabled() {
    this.updateAddonsList();
  }

  render() {
    const { client, connect, id } = this.props;
    const { debugDisabled, extensions: targets, showSystemAddons } = this.state;
    const installedName = Strings.GetStringFromName("extensions");
    const temporaryName = Strings.GetStringFromName("temporaryExtensions");
    const systemName = Strings.GetStringFromName("systemExtensions");
    const targetClass = AddonTarget;

    const installedTargets = targets.filter(
      (target) => !target.isSystem && !target.temporarilyInstalled);
    const temporaryTargets = targets.filter((target) => target.temporarilyInstalled);
    const systemTargets = showSystemAddons && targets.filter((target) => target.isSystem);

    return dom.div({
      id: id + "-panel",
      className: "panel",
      role: "tabpanel",
      "aria-labelledby": id + "-header",
    },
    PanelHeader({
      id: id + "-header",
      name: Strings.GetStringFromName("addons"),
    }),
    AddonsControls({ debugDisabled }),
    dom.div({ id: "temporary-addons" },
      TargetList({
        id: "temporary-extensions",
        name: temporaryName,
        targets: temporaryTargets,
        client,
        connect,
        debugDisabled,
        targetClass,
        sort: true,
      }),
      dom.div({ className: "addons-tip"},
        dom.div({ className: "addons-tip-icon"}),
        dom.span({
          className: "addons-web-ext-tip",
        }, Strings.GetStringFromName("webExtTip")),
        dom.a({ href: WEB_EXT_URL, target: "_blank" },
          Strings.GetStringFromName("webExtTip.learnMore")
        )
      )
    ),
    dom.div({ id: "addons" },
      TargetList({
        id: "extensions",
        name: installedName,
        targets: installedTargets,
        client,
        connect,
        debugDisabled,
        targetClass,
        sort: true,
      })
    ),
    showSystemAddons ?
      dom.div({ id: "system-addons" },
        TargetList({
          id: "system-extensions",
          name: systemName,
          targets: systemTargets,
          client,
          connect,
          debugDisabled,
          targetClass,
          sort: true,
        })
      ) : null,
    );
  }
}

module.exports = AddonsPanel;
