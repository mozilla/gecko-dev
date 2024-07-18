/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Imports necessary modules from ChromeUtils.
 */
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  IndexedDBCache: "chrome://global/content/ml/ModelHub.sys.mjs",
  ModelHub: "chrome://global/content/ml/ModelHub.sys.mjs",
  detectSimdSupport: "chrome://global/content/ml/Utils.sys.mjs",
  getRuntimeWasmFilename: "chrome://global/content/ml/Utils.sys.mjs",
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
});

/**
 * Preferences for machine learning enablement and model hub configuration.
 */
const ML_ENABLE = Services.prefs.getBoolPref("browser.ml.enable");
const MODEL_HUB_ROOT_URL = Services.prefs.getStringPref(
  "browser.ml.modelHubRootUrl"
);
const MODEL_HUB_URL_TEMPLATE = Services.prefs.getStringPref(
  "browser.ml.modelHubUrlTemplate"
);

let modelHub = null;
let modelCache = null;

/**
 * Gets an instance of ModelHub. Initializes it if it doesn't already exist.
 *
 * @returns {ModelHub} The ModelHub instance.
 */
function getModelHub() {
  if (!modelHub) {
    modelHub = new lazy.ModelHub({
      rootUrl: MODEL_HUB_ROOT_URL,
      urlTemplate: MODEL_HUB_URL_TEMPLATE,
    });
  }
  return modelHub;
}

/**
 * Formats a number of bytes into a human-readable string.
 *
 * @param {number} bytes - The number of bytes to format.
 * @returns {string} The formatted string.
 */
function formatBytes(bytes) {
  const size = lazy.DownloadUtils.convertByteUnits(bytes);
  return `${size[0]} ${size[1]}`;
}

/**
 * Displays process information in a table. Only includes processes of type "inference".
 *
 * @async
 */
async function displayProcessInfo() {
  let info = await ChromeUtils.requestProcInfo();
  let tableContainer = document.getElementById("runningInference");
  let fragment = document.createDocumentFragment();
  let table = document.createElement("table");
  table.border = "1";
  let thead = document.createElement("thead");
  let headerRow = document.createElement("tr");
  let th1 = document.createElement("th");
  document.l10n.setAttributes(th1, "about-inference-pid");
  headerRow.appendChild(th1);
  let th2 = document.createElement("th");
  document.l10n.setAttributes(th2, "about-inference-memory");
  headerRow.appendChild(th2);

  thead.appendChild(headerRow);
  table.appendChild(thead);

  let foundInference = false;
  let tbody = document.createElement("tbody");

  for (const child of info.children) {
    if (child.type === "inference") {
      foundInference = true;
      let row = document.createElement("tr");

      let pidCell = document.createElement("td");
      pidCell.textContent = child.pid;
      row.appendChild(pidCell);

      let memoryCell = document.createElement("td");
      memoryCell.textContent = formatBytes(child.memory);
      row.appendChild(memoryCell);

      tbody.appendChild(row);
    }
  }

  table.appendChild(tbody);

  if (foundInference) {
    table.appendChild(tbody);
    fragment.appendChild(table);
  } else {
    let noneLabel = document.createElement("div");
    document.l10n.setAttributes(noneLabel, "about-inference-no-processes");
    fragment.appendChild(noneLabel);
  }

  tableContainer.innerHTML = "";
  tableContainer.appendChild(fragment);
}

/**
 * Displays information about the machine learning models and process info.
 *
 * @async
 */
async function displayInfo() {
  if (!ML_ENABLE) {
    let warning = document.getElementById("warning");
    warning.style.display = "block";
  }

  let cache = await lazy.IndexedDBCache.init();
  let models = await cache.listModels();
  let modelFilesDiv = document.getElementById("modelFiles");

  // Use DocumentFragment to avoid reflows
  let fragment = document.createDocumentFragment();

  for (const entry of models) {
    let files = await cache.listFiles(entry.name, entry.revision);

    // Create a new table for the current model
    let table = document.createElement("table");

    // caption block
    let caption = document.createElement("caption");
    let modelInfo = document.createElement("div");
    modelInfo.textContent = `${entry.name} (${entry.revision})`;
    let deleteButton = document.createElement("button");
    document.l10n.setAttributes(deleteButton, "about-inference-delete-button");
    deleteButton.onclick = async () => {
      await cache.deleteModel(entry.name, entry.revision);
      modelFilesDiv.removeChild(table); // Remove the table from the DOM
    };

    modelInfo.appendChild(deleteButton);
    caption.appendChild(modelInfo);
    table.appendChild(caption);

    // Create table headers
    let thead = document.createElement("thead");
    let headerRow = document.createElement("tr");
    let thFile = document.createElement("th");
    document.l10n.setAttributes(thFile, "about-inference-file");
    headerRow.appendChild(thFile);
    thFile = document.createElement("th");
    document.l10n.setAttributes(thFile, "about-inference-size");
    headerRow.appendChild(thFile);
    thead.appendChild(headerRow);
    table.appendChild(thead);

    // Create table body
    let tbody = document.createElement("tbody");
    let totalSize = 0;

    for (const file of files) {
      let row = document.createElement("tr");
      let tdFile = document.createElement("td");
      tdFile.textContent = file.path;
      row.appendChild(tdFile);
      const fileSize = parseInt(
        file.headers.fileSize || file.headers["Content-Length"] || 0
      );

      tdFile = document.createElement("td");
      tdFile.textContent = formatBytes(fileSize);
      row.appendChild(tdFile);
      tbody.appendChild(row);
      totalSize += fileSize;
    }

    // Append the total line
    let totalRow = document.createElement("tr");
    let tdTotalLabel = document.createElement("td");
    document.l10n.setAttributes(tdTotalLabel, "about-inference-total");
    totalRow.appendChild(tdTotalLabel);

    let tdTotalValue = document.createElement("td");
    tdTotalValue.textContent = formatBytes(totalSize);
    totalRow.appendChild(tdTotalValue);
    tbody.appendChild(totalRow);

    table.appendChild(tbody);
    fragment.appendChild(table);
  }

  modelFilesDiv.innerHTML = "";
  modelFilesDiv.appendChild(fragment);

  await displayProcessInfo();
  document.getElementById("onnxRuntime").textContent =
    lazy.getRuntimeWasmFilename();

  if (lazy.detectSimdSupport()) {
    document.l10n.setAttributes(
      document.getElementById("onnxSimd"),
      "about-inference-yes"
    );
  } else {
    document.l10n.setAttributes(
      document.getElementById("onnxSimd"),
      "about-inference-no"
    );
  }
}

/**
 * Initializes the display of information when the window loads and sets an interval to update it.
 *
 * @async
 */
window.onload = async function () {
  await displayInfo();
  setInterval(displayInfo, 5000);
};
