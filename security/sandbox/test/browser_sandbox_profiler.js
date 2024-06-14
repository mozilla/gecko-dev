/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from /tools/profiler/tests/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/tools/profiler/tests/browser/shared-head.js",
  this
);

async function addTab() {
  const tab = BrowserTestUtils.addTab(gBrowser, "https://example.com/browser", {
    forceNewProcess: true,
  });
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);
  return tab;
}

const sandboxSettings = {
  entries: 8 * 1024 * 1024, // 8M entries = 64MB
  interval: 1, // ms
  features: ["stackwalk"],
  threads: ["SandboxProfilerEmitter"],
};

const kNewProcesses = 2;

async function waitForSandboxProfilerData(threadName, name1, withStacks) {
  let tabs = [];
  for (let i = 0; i < kNewProcesses; ++i) {
    tabs.push(await addTab());
  }

  let profile;
  let intercepted = undefined;
  await TestUtils.waitForCondition(
    async () => {
      profile = await Services.profiler.getProfileDataAsync();
      intercepted = profile.processes
        .flatMap(ps => {
          let sandboxThreads = ps.threads.filter(th => th.name === threadName);
          return sandboxThreads.flatMap(th => {
            let markersData = th.markers.data;
            return markersData.flatMap(d => {
              let [, , , , , o] = d;
              return o;
            });
          });
        })
        .filter(x => "name1" in x && name1.includes(x.name1) >= 0);
      return !!intercepted.length;
    },
    `Wait for some samples from ${threadName}`,
    /* interval*/ 250,
    /* maxTries */ 75
  );
  Assert.greater(
    intercepted.length,
    0,
    `Should have collected some data from ${threadName}`
  );

  if (withStacks) {
    let stacks = profile.processes.flatMap(ps => {
      let sandboxThreads = ps.threads.filter(th => th.name === threadName);
      return sandboxThreads.flatMap(th => {
        let stackTableData = th.stackTable.data;
        return stackTableData.flatMap(d => {
          return [d];
        });
      });
    });
    Assert.greater(stacks.length, 0, "Should have some stack as well");
  }

  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }
}

add_task(async () => {
  await startProfiler(sandboxSettings);

  await waitForSandboxProfilerData(
    "SandboxProfilerEmitterSyscalls",
    ["id", "init"],
    true
  );

  await waitForSandboxProfilerData(
    "SandboxProfilerEmitterLogs",
    ["log"],
    false
  );

  await Services.profiler.StopProfiler();
});

add_task(async () => {});
