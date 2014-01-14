/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests debug button for addons in list view
 */

const getDebugButton = node =>
    node.ownerDocument.getAnonymousElementByAttribute(node,
                                                      "anonid",
                                                      "debug-btn");

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();


  var gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "non-debuggable@tests.mozilla.org",
    name: "No debug",
    description: "foo"
  },
  {
    id: "debuggable@tests.mozilla.org",
    name: "Debuggable",
    description: "bar",
    isDebuggable: true
  }]);

  // Enable add-on debugger
  Services.prefs.setBoolPref("devtools.debugger.addon-enabled", true);

  open_manager("addons://list/extension", function(aManager) {
    const {document} = aManager;
    const addonList = document.getElementById("addon-list");
    const nondebug = addonList.querySelector("[name='No debug']");
    const debuggable = addonList.querySelector("[name='Debuggable']");

    is(getDebugButton(nondebug).disabled, true,
       "button is disabled for legacy addons");

    is(getDebugButton(debuggable).disabled, false,
       "button is enabled for debuggable addons");

    close_manager(aManager, finish);
  });
}
