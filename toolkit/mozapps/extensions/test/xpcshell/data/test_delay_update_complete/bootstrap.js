Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");

const ADDON_ID = "test_delay_update_complete@tests.mozilla.org";
const INSTALL_COMPLETE_PREF = "bootstraptest.install_complete_done";

function install(data, reason) {}

// normally we would use BootstrapMonitor here, but we need a reference to
// the symbol inside `XPIProvider.jsm`.
function startup(data, reason) {
  // apply update immediately
  if (data.hasOwnProperty("instanceID") && data.instanceID) {
    AddonManager.addUpgradeListener(data.instanceID, (upgrade) => {
      upgrade.install();
    });
  } else {
    throw Error("no instanceID passed to bootstrap startup");
  }
}

function shutdown(data, reason) {}

function uninstall(data, reason) {}
