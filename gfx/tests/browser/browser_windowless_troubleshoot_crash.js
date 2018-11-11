let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

add_task(async function test_windowlessBrowserTroubleshootCrash() {
  let webNav = Services.appShell.createWindowlessBrowser(false);

  let onLoaded = new Promise((resolve, reject) => {
    let docShell = webNav.docShell;
    let listener = {
      observe(contentWindow, topic, data) {
        let observedDocShell = contentWindow.docShell
                                            .sameTypeRootTreeItem
                                            .QueryInterface(Ci.nsIDocShell);
          if (docShell === observedDocShell) {
            Services.obs.removeObserver(listener, "content-document-global-created");
            resolve();
          }
        }
    }
    Services.obs.addObserver(listener, "content-document-global-created");
  });
  let triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal({});
  webNav.loadURI("about:blank", 0, null, null, null, triggeringPrincipal);

  await onLoaded;

  let winUtils = webNav.document.defaultView.windowUtils;
  try {
    is(winUtils.layerManagerType, "Basic", "windowless browser's layerManagerType should be 'Basic'");
  } catch (e) {
    // The windowless browser may not have a layermanager at all yet, and that's ok.
    // The troubleshooting code similarly skips over windows with no layer managers.
  }
  ok(true, "not crashed");

  var Troubleshoot = ChromeUtils.import("resource://gre/modules/Troubleshoot.jsm", {}).Troubleshoot;
  var data = await new Promise((resolve, reject) => {
    Troubleshoot.snapshot((data) => {
      resolve(data);
    });
  });

  ok(data.graphics.windowLayerManagerType !== "None", "windowless browser window should not set windowLayerManagerType to 'None'");

  webNav.close();
});
