/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test can be slow on linux debug
requestLongerTimeout(2);

// As we are closing devtools while it may still be loading,
// we are getting various exception about pending request still in flight while closing
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);

// Bug 1898490: DevTools may prevents opening when some random content process is being destroyed
// in middle of DevTools initialization.
// So try opening DevTools a couple of times while destroying content processes in the background.

const URL1 =
  "data:text/html;charset=utf8,test many toggles with other content process destructions";

add_task(
  async function manyTogglesWithContentProcessDestructionsInBackground() {
    const tab = await addTab(URL1);

    const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
      Ci.nsIProcessToolsService
    );

    const openedTabs = [];
    const interval = setInterval(() => {
      // Close the process specific to about:home, which is using a privilegedabout process type
      const pid = ChromeUtils.getAllDOMProcesses().filter(
        r => r.remoteType == "privilegedabout"
      )[0]?.osPid;
      if (!pid) {
        return;
      }

      try {
        ProcessTools.kill(pid);
      } catch (ex) {
        info(`ProcessTools.kill(${pid}) returned: ${ex.result}`);

        // NS_ERROR_NOT_AVAILABLE is thrown is the process disappeared (ESRCH)
        // So we should be fine ignoring this
        if (ex.result !== Cr.NS_ERROR_NOT_AVAILABLE) {
          throw ex;
        }
      }

      // The privilegedabout process wouldn't be automatically re-created, so open a new tab to force creating a new process.
      openedTabs.push(BrowserTestUtils.addTab(gBrowser, "about:home"));
    }, 10);

    info(
      "Open/close DevTools many times in a row while some processes get destroyed"
    );
    for (let i = 0; i < 5; i++) {
      const toolbox = await gDevTools.showToolboxForTab(tab, {
        toolId: "webconsole",
      });
      await toolbox.destroy();
    }

    clearInterval(interval);

    info("Close all tabs that were used to spawn a new content process");
    for (const tab of openedTabs) {
      await removeTab(tab);
    }

    await removeTab(tab);
  }
);

// Bug 1903980: DevTools throw in background and becomes blank when closing them while iframe are being destroyed.
const URL2 = `data:text/html,Test toggling DevTools with many destroying iframes`;

add_task(async function manyTogglesWithDestroyingIframes() {
  const tab = await addTab(URL2);

  // Run the infinite loop creating iframe *after* having called addTab
  // as it may confuse the test helper and make it consider the page as still loading.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    let iframe;
    content.interval = content.setInterval(function () {
      if (iframe) {
        iframe.remove();
      }
      iframe = content.document.createElement("iframe");
      iframe.src = "data:text/html,foo";
      content.document.body.appendChild(iframe);

      // Do not use a timeout lower than 100 as it would freeze all executions with MOZ_CHAOS mode
      // If the interval is lower than 50, no RDP message would be able to be emitted
      // and between 50 and 100, only frameUpdate would be notified without allowing to load the toolbox
    }, 100);
  });

  info(
    "Open/close DevTools many times in a row while some processes get destroyed"
  );
  for (let i = 0; i < 3; i++) {
    info(` # Toggle DevTools attempt #${i}`);
    const toolbox = await gDevTools.showToolboxForTab(tab, {
      toolId: "webconsole",
    });
    await toolbox.destroy();
  }

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.clearInterval(content.interval);
  });

  await removeTab(tab);
});
