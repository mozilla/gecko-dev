/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test can be slow on linux debug
requestLongerTimeout(2);

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
      ProcessTools.kill(pid);
      // The privilegedabout process wouldn't be automatically re-created, so open a new tab to force creating a new process.
      openedTabs.push(BrowserTestUtils.addTab(gBrowser, "about:home"));
    });

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
const URL2 = `data:text/html,<script>
    setInterval(()=>{
      document.body.innerHTML="";
      let iframe=document.createElement("iframe");
      iframe.src="data:text/html,foo";
      document.body.appendChild(iframe);
    },0);</script>`;

add_task(async function manyTogglesWithDestroyingIframes() {
  const tab = await addTab(URL2);

  info(
    "Open/close DevTools many times in a row while some processes get destroyed"
  );
  for (let i = 0; i < 3; i++) {
    const toolbox = await gDevTools.showToolboxForTab(tab, {
      toolId: "webconsole",
    });
    await toolbox.destroy();
  }

  await removeTab(tab);
});
