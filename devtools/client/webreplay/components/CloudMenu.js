/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const Services = require("Services");
const { E10SUtils } = require("resource://gre/modules/E10SUtils.jsm");

loader.lazyRequireGetter(
  this,
  "HTMLTooltip",
  "devtools/client/shared/widgets/tooltip/HTMLTooltip",
  true
);
loader.lazyRequireGetter(
  this,
  "gDevToolsBrowser",
  "devtools/client/framework/devtools-browser",
  true
);

const CloudMenu = {
  // Logic styled after devtools-fission-prefs.js
  showMenu(toolbox) {
    if (!toolbox._cloudReplayTooltip) {
      toolbox._cloudReplayTooltip = new HTMLTooltip(toolbox.doc, {
        type: "doorhanger",
        useXulWrapper: true,
      });
      toolbox.once("destroy", () => toolbox._cloudReplayTooltip.destroy());
    }

    if (toolbox._cloudReplayTooltip.preventShow) {
      return;
    }

    const container = toolbox.doc.createElement("div");
    container.style.padding = "12px";
    container.style.fontSize = "11px";
    container.classList.add("theme-body");

    const recordingName = toolbox.doc.createElement("input");
    recordingName.type = "input";
    container.appendChild(recordingName);

    container.appendChild(toolbox.doc.createElement("br"));

    function validateRecordingName() {
      const name = recordingName.value;
      if (!name) {
        toolbox.topWindow.alert("Need recording name");
        return null;
      }

      // Recording names are embedded in build IDs and have a maximum length.
      if (name.length > 100) {
        toolbox.topWindow.alert("Recording name too long");
        return null;
      }

      return name;
    }

    const saveRecording = toolbox.doc.createElement("button");
    saveRecording.textContent = "Save Recording";
    saveRecording.onclick = () => {
      const name = validateRecordingName();
      if (name) {
        const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
        const remoteTab =
              gBrowser.selectedTab.linkedBrowser.frameLoader.remoteTab;
        if (!remoteTab || !remoteTab.saveCloudRecording(name)) {
          toolbox.topWindow.alert("Current tab is not recording");
        }
      }
    };
    container.appendChild(saveRecording);

    container.appendChild(toolbox.doc.createElement("br"));

    const loadRecording = toolbox.doc.createElement("button");
    loadRecording.textContent = "Load Recording";
    loadRecording.onclick = () => {
      const name = validateRecordingName();
      if (name) {
        const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
        const tab = gBrowser.selectedTab;
        gBrowser.selectedTab = gBrowser.addWebTab(null, {
          replayExecution: `cloud-replay://${name}`,
        });
        gBrowser.removeTab(tab);
        gDevToolsBrowser.toggleToolboxCommand(gBrowser, Cu.now());
      }
    };
    container.appendChild(loadRecording);

    toolbox._cloudReplayTooltip.panel.innerHTML = "";
    toolbox._cloudReplayTooltip.panel.appendChild(container);

    const commandId = "command-button-replay-cloud";
    toolbox._cloudReplayTooltip.show(toolbox.doc.getElementById(commandId));

    toolbox._cloudReplayTooltip.preventShow = true;
    toolbox._cloudReplayTooltip.once("hidden", () => {
      toolbox.win.setTimeout(
        () => (toolbox._cloudReplayTooltip.preventShow = false),
        250
      );
    });
  },
};

module.exports = CloudMenu;
