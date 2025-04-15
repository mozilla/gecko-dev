/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test covers changing to a distinct "project root"
// i.e. displaying only one particular thread, domain, or directory in the Source Tree.

"use strict";

const httpServer = createTestHTTPServer();
const HOST = `localhost:${httpServer.identity.primaryPort}`;
const BASE_URL = `http://${HOST}/`;

const PAGE_URL = BASE_URL + "index.html";
const PAGE_CONTENT = `<!DOCTYPE html>
  <html>
    <head>
      <script type="text/javascript" src="/root-script.js"></script>
      <script type="text/javascript" src="/folder/folder-script.js"></script>
      <script type="text/javascript" src="/folder/sub-folder/sub-folder-script.js"></script>
    </head>
    <body></body>
  </html>`;
const ALL_PAGE_SCRIPTS = [
  "root-script.js",
  "folder-script.js",
  "sub-folder-script.js",
];

httpServer.registerPathHandler("/index.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(PAGE_CONTENT);
});
httpServer.registerPathHandler("/root-script.js", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.write("console.log('root script')");
});
httpServer.registerPathHandler(
  "/folder/folder-script.js",
  (request, response) => {
    response.setHeader("Content-Type", "application/javascript");
    response.write("console.log('folder script')");
  }
);
httpServer.registerPathHandler(
  "/folder/sub-folder/sub-folder-script.js",
  (request, response) => {
    response.setHeader("Content-Type", "application/javascript");
    response.write("console.log('sub folder script')");
  }
);

const PAGE2_CONTENT = `<!DOCTYPE html>
  <html>
    <head>
      <script type="text/javascript" src="/src/script.js"></script>
      <script type="text/javascript">
        console.log("webpack script");
        //# sourceURL=webpack:///src/webpack-script.js
      </script>
      <script type="text/javascript">
        console.log("turbopack script");
        //# sourceURL=turbopack:///src/turbopack-script.js
      </script>
      <script type="text/javascript">
        console.log("angular script");
        //# sourceURL=ng:///src/angular-script.js
      </script>
      <script type="text/javascript">
        console.log("resource script");
        //# sourceURL=resource://devtools/test/resource-script.js
      </script>
    </head>
    <body></body>
  </html>`;
const ALL_PAGE2_SCRIPTS = [
  "script.js",
  "webpack-script.js",
  "turbopack-script.js",
  "angular-script.js",
  "resource-script.js",
  "worker-script.js",
];

httpServer.registerPathHandler("/index2.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(PAGE2_CONTENT);
});
httpServer.registerPathHandler("/src/script.js", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.write(
    "console.log('src script'); const worker = new Worker('src/worker-script.js')"
  );
});
httpServer.registerPathHandler("/src/worker-script.js", (request, response) => {
  response.setHeader("Content-Type", "application/javascript");
  response.write("console.log('worker script')");
});

add_task(async function testProjectRoot() {
  await pushPref("devtools.debugger.show-content-scripts", true);

  const dbg = await initDebuggerWithAbsoluteURL(PAGE_URL, ...ALL_PAGE_SCRIPTS);

  await waitForSourcesInSourceTree(dbg, ALL_PAGE_SCRIPTS);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "Main Thread",
      tooltip: "Main Thread",
      sources: ALL_PAGE_SCRIPTS,
    },
    {
      label: HOST,
      tooltip: `${BASE_URL.slice(0, -1)} on Main Thread`,
      sources: ALL_PAGE_SCRIPTS,
    },
    {
      label: "folder",
      tooltip: `${BASE_URL}folder on Main Thread`,
      sources: ["folder-script.js", "sub-folder-script.js"],
    },
  ]);

  info("Reload and see if project root is preserved");
  await reload(dbg, "folder-script.js", "sub-folder-script.js");
  await checkProjectRoot(dbg, "folder", `${BASE_URL}folder on Main Thread`, [
    "folder-script.js",
    "sub-folder-script.js",
  ]);

  info("Select 'sub-folder' as project root");
  await selectAndCheckProjectRoots(dbg, [
    {
      label: "sub-folder",
      tooltip: `${BASE_URL}folder/sub-folder on Main Thread`,
      sources: ["sub-folder-script.js"],
    },
  ]);

  info("Navigate to a different page");
  await navigateTo(BASE_URL + "index2.html");
  await checkProjectRoot(
    dbg,
    "sub-folder",
    `${BASE_URL}folder/sub-folder on Main Thread`,
    []
  );

  info("Clear project root");
  await clearProjectRoot(dbg);
  await waitForSourcesInSourceTree(dbg, ALL_PAGE2_SCRIPTS);
  checkNoProjectRoot(dbg);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "Webpack",
      tooltip: `webpack:// on Main Thread`,
      sources: ["webpack-script.js"],
    },
    {
      label: "src",
      tooltip: `webpack:///src on Main Thread`,
      sources: ["webpack-script.js"],
    },
  ]);

  info("Clear project root");
  await clearProjectRoot(dbg);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "turbopack://",
      tooltip: `turbopack:// on Main Thread`,
      sources: ["turbopack-script.js"],
    },
    {
      label: "src",
      tooltip: `turbopack:///src on Main Thread`,
      sources: ["turbopack-script.js"],
    },
  ]);

  info("Clear project root");
  await clearProjectRoot(dbg);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "Angular",
      tooltip: `ng:// on Main Thread`,
      sources: ["angular-script.js"],
    },
    {
      label: "src",
      tooltip: `ng:///src on Main Thread`,
      sources: ["angular-script.js"],
    },
  ]);

  info("Clear project root");
  await clearProjectRoot(dbg);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "resource://devtools",
      tooltip: `resource://devtools on Main Thread`,
      sources: ["resource-script.js"],
    },
    {
      label: "test",
      tooltip: `resource://devtools/test on Main Thread`,
      sources: ["resource-script.js"],
    },
  ]);

  info("Clear project root");
  await clearProjectRoot(dbg);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "worker-script.js",
      tooltip: `worker-script.js`,
      sources: ["worker-script.js"],
    },
    {
      label: HOST,
      tooltip: `${BASE_URL.slice(0, -1)} on worker-script.js`,
      sources: ["worker-script.js"],
    },
    {
      label: "src",
      tooltip: `${BASE_URL}src on worker-script.js`,
      sources: ["worker-script.js"],
    },
  ]);

  info("Clear project root");
  await clearProjectRoot(dbg);

  info("Load the test extension");
  const extension = await installAndStartExtension();
  await waitForSourcesInSourceTree(dbg, [
    ...ALL_PAGE2_SCRIPTS,
    "content_script.js",
  ]);

  await selectAndCheckProjectRoots(dbg, [
    {
      label: "Test extension",
      tooltip: `Test extension`,
      sources: ["content_script.js"],
    },
    {
      label: "Test extension",
      tooltip: `moz-extension://${extension.uuid} on Test extension`,
      sources: ["content_script.js"],
    },
    {
      label: "src",
      tooltip: `moz-extension://${extension.uuid}/src on Test extension`,
      sources: ["content_script.js"],
    },
  ]);

  await extension.unload();

  info("Clear project root");
  await clearProjectRoot(dbg);

  info("Navigate to a data: URL");
  const dataURL =
    "data:text/html,<meta charset=utf8><script>console.log('inline script')</script>";
  await navigateTo(dataURL);
  const noDomainItem = findSourceNodeWithText(dbg, "(no domain)");
  await setProjectRoot(dbg, noDomainItem);
  await checkProjectRoot(dbg, "(no domain)", `data: on Main Thread`, [dataURL]);
});

async function setProjectRoot(dbg, treeNode) {
  const dispatched = waitForDispatch(dbg.store, "SET_PROJECT_DIRECTORY_ROOT");
  await triggerSourceTreeContextMenu(dbg, treeNode, "#node-set-directory-root");
  await dispatched;
}

async function checkProjectRoot(dbg, label, tooltip, sources) {
  assertRootLabel(dbg, label);
  assertRootLabelTooltip(dbg, `Directory root set to ${tooltip}`);
  if (sources.length) {
    await waitForSourcesInSourceTree(dbg, sources);
  } else {
    ok(dbg.win.document.querySelector(".no-sources-message"));
  }
}

async function selectAndCheckProjectRoots(dbg, tests) {
  for (const test of tests) {
    const { label, tooltip, sources } = test;

    info(`Select ${label} as project root`);
    const item = findSourceNodeWithText(dbg, label);
    await setProjectRoot(dbg, item);

    await checkProjectRoot(dbg, label, tooltip, sources);
  }
}

async function checkNoProjectRoot(dbg) {
  ok(!dbg.win.document.querySelector(".sources-clear-root"));
}

function assertRootLabel(dbg, label) {
  const rootHeaderLabel = dbg.win.document.querySelector(
    ".sources-clear-root-label"
  );
  is(rootHeaderLabel.textContent, label);
}

function assertRootLabelTooltip(dbg, text) {
  const rootHeader = dbg.win.document.querySelector(
    ".sources-clear-root-label"
  );
  ok(rootHeader.title.includes(text));
}

async function clearProjectRoot(dbg) {
  const rootHeader = dbg.win.document.querySelector(".sources-clear-root");
  rootHeader.click();
}

async function installAndStartExtension() {
  function contentScript() {
    console.log("content script loads");

    // This listener prevents the source from being garbage collected
    // and be missing from the scripts returned by `dbg.findScripts()`
    // in `ThreadActor._discoverSources`.
    window.onload = () => {};
  }

  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      content_scripts: [
        {
          js: ["src/content_script.js"],
          matches: ["http://*/*"],
          run_at: "document_start",
        },
      ],
    },
    useAddonManager: "temporary",
    files: {
      "src/content_script.js": contentScript,
    },
  });

  await extension.startup();

  return extension;
}
