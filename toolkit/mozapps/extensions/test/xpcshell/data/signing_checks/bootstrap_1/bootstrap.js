Components.utils.import("resource://gre/modules/Services.jsm");

const VERSION = 1;

// Test steps chain from pref observers on *_reason,
// so always set that last
function install(data, reason) {
  Services.prefs.setIntPref("bootstraptest.installed_version", VERSION);
  Services.prefs.setIntPref("bootstraptest.install_oldversion", data.oldVersion);
  Services.prefs.setIntPref("bootstraptest.install_reason", reason);
}

function startup(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", VERSION);
  Services.prefs.setIntPref("bootstraptest.startup_oldversion", data.oldVersion);
  Services.prefs.setIntPref("bootstraptest.startup_reason", reason);
}

function shutdown(data, reason) {
  Services.prefs.setIntPref("bootstraptest.active_version", 0);
  Services.prefs.setIntPref("bootstraptest.shutdown_newversion", data.newVersion);
  Services.prefs.setIntPref("bootstraptest.shutdown_reason", reason);
}

function uninstall(data, reason) {
  Services.prefs.setIntPref("bootstraptest.installed_version", 0);
  Services.prefs.setIntPref("bootstraptest.uninstall_newversion", data.newVersion);
  Services.prefs.setIntPref("bootstraptest.uninstall_reason", reason);
}
