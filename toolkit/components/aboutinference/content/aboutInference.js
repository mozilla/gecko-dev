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
  DownloadUtils: "resource://gre/modules/DownloadUtils.sys.mjs",
  HttpInference: "chrome://global/content/ml/HttpInference.sys.mjs",
  IndexedDBCache: "chrome://global/content/ml/ModelHub.sys.mjs",
  ModelHub: "chrome://global/content/ml/ModelHub.sys.mjs",
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

const TASKS = [
  "text-classification",
  "token-classification",
  "question-answering",
  "fill-mask",
  "summarization",
  "translation",
  "text2text-generation",
  "text-generation",
  "zero-shot-classification",
  "audio-classification",
  "zero-shot-audio-classification",
  "automatic-speech-recognition",
  "text-to-audio",
  "image-to-text",
  "image-classification",
  "image-segmentation",
  "zero-shot-image-classification",
  "object-detection",
  "zero-shot-object-detection",
  "document-question-answering",
  "image-to-image",
  "depth-estimation",
  "feature-extraction",
  "image-feature-extraction",
];

const DTYPE = ["fp32", "fp16", "q8", "int8", "uint8", "q4", "bnb4", "q4f16"];

/**
 * Presets for the pad
 */
const INFERENCE_PAD_PRESETS = {
  "image-to-text": {
    inputArgs: [
      "https://huggingface.co/datasets/mishig/sample_images/resolve/main/football-match.jpg",
    ],
    runOptions: {},
    task: "image-to-text",
    modelId: "mozilla/distilvit",
    modelRevision: "main",
    modelHub: "hf",
    dtype: "q8",
    device: "wasm",
  },
  ner: {
    inputArgs: ["Sarah lives in the United States of America"],
    runOptions: {},
    task: "token-classification",
    modelId: "Xenova/bert-base-NER",
    modelRevision: "main",
    modelHub: "hf",
    dtype: "q8",
    device: "wasm",
  },
  summary: {
    inputArgs: [
      "The tower is 324 metres (1,063 ft) tall, about the same height as an 81-storey building, and the tallest structure in Paris. Its base is square, measuring 125 metres (410 ft) on each side. During its construction, the Eiffel Tower surpassed the Washington Monument to become the tallest man-made structure in the world, a title it held for 41 years until the Chrysler Building in New York City was finished in 1930. It was the first structure to reach a height of 300 metres. Due to the addition of a broadcasting aerial at the top of the tower in 1957, it is now taller than the Chrysler Building by 5.2 metres (17 ft). Excluding transmitters, the Eiffel Tower is the second tallest free-standing structure in France after the Millau Viaduct.",
    ],
    runOptions: {
      max_new_tokens: 100,
    },
    task: "summarization",
    modelId: "Xenova/long-t5-tglobal-base-16384-book-summary",
    modelRevision: "main",
    modelHub: "hf",
    dtype: "q8",
    device: "wasm",
  },
  zero: {
    inputArgs: [
      "Last week I upgraded my iOS version and ever since then my phone has been overheating whenever I use your app.",
      ["mobile", "billing", "website", "account access"],
    ],
    runOptions: {},
    task: "zero-shot-classification",
    modelId: "Xenova/mobilebert-uncased-mnli",
    modelRevision: "main",
    modelHub: "hf",
    dtype: "q8",
    device: "wasm",
  },
};

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
    document.getElementById("content").style.display = "none";
  } else {
    document.getElementById("content").style.display = "block";
  }

  let cache = await lazy.IndexedDBCache.init();
  let models = await cache.listModels();
  let modelFilesDiv = document.getElementById("modelFiles");

  // Use DocumentFragment to avoid reflows
  let fragment = document.createDocumentFragment();

  for (const { name: model, revision } of models) {
    let files = await cache.listFiles({ model, revision });

    // Create a new table for the current model
    let table = document.createElement("table");

    // caption block
    let caption = document.createElement("caption");
    let modelInfo = document.createElement("div");
    modelInfo.textContent = `${model} (${revision})`;
    let deleteButton = document.createElement("button");
    document.l10n.setAttributes(deleteButton, "about-inference-delete-button");
    deleteButton.onclick = async () => {
      await cache.deleteModels({ model, revision });
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
}

function setSelectOption(selectId, optionValue) {
  const selectElement = document.getElementById(selectId);
  if (!selectElement) {
    console.error(`No select element found with ID: ${selectId}`);
    return;
  }

  const options = selectElement.options;
  if (!options) {
    console.error(`No options found for select element with ID: ${selectId}`);
    return;
  }

  for (let i = 0; i < options.length; i++) {
    if (options[i].value === optionValue) {
      selectElement.selectedIndex = i;
      return;
    }
  }

  console.warn(`No option found with value: ${optionValue}`);
}

function loadExample(name) {
  let data = INFERENCE_PAD_PRESETS[name];
  let padContent = { inputArgs: data.inputArgs, runOptions: data.runOptions };
  document.getElementById("inferencePad").value = JSON.stringify(
    padContent,
    null,
    2
  );
  setSelectOption("taskName", data.task);
  document.getElementById("modelId").value = data.modelId;
  document.getElementById("modelRevision").value = data.modelRevision;
  setSelectOption("modelHub", data.modelHub);
  setSelectOption("dtype", data.dtype);
  setSelectOption("device", data.device);
}

function formatJSON() {
  const textarea = document.getElementById("inferencePad");
  const jsonInput = textarea.value;

  const jsonObject = JSON.parse(jsonInput);
  const formattedJson = JSON.stringify(jsonObject, null, 2); // Pretty-print with 2 spaces
  textarea.value = formattedJson;
}

async function runInference() {
  document.getElementById("console").value = "";
  const inferencePadValue = document.getElementById("inferencePad").value;
  const modelId = document.getElementById("modelId").value;
  const modelRevision = document.getElementById("modelRevision").value;
  const taskName = document.getElementById("taskName").value;
  const dtype = document.getElementById("dtype").value;
  const device = document.getElementById("device").value;

  let inputData;
  try {
    inputData = JSON.parse(inferencePadValue);
  } catch (error) {
    alert("Invalid JSON input");
    return;
  }

  const modelHubRootUrl = "https://huggingface.co";
  const modelHubUrlTemplate = "{model}/resolve/{revision}";

  const initData = {
    modelId,
    modelRevision,
    tokenizerRevision: modelRevision,
    processorRevision: modelRevision,
    tokenizerId: modelId,
    processorId: modelId,
    taskName,
    engineId: "about:inference",
    modelHubRootUrl,
    modelHubUrlTemplate,
    device,
    dtype,
  };

  const { createEngine } = ChromeUtils.importESModule(
    "chrome://global/content/ml/EngineProcess.sys.mjs"
  );

  appendTextConsole("Creating engine if needed");
  let engine;
  try {
    engine = await createEngine(initData, appendConsole);
  } catch (e) {
    appendTextConsole(e);
    throw e;
  }

  appendTextConsole("Running inference request");

  const request = { args: inputData.inputArgs, options: inputData.runOptions };

  let res;

  try {
    res = await engine.run(request);
  } catch (e) {
    appendTextConsole(e);
    if (
      e.message.includes("Invalid model hub root url: https://huggingface.co")
    ) {
      appendTextConsole(
        "Make sure you started Firefox with MOZ_ALLOW_EXTERNAL_ML_HUB=1"
      );
    }

    throw e;
  }

  appendTextConsole(`Results: ${JSON.stringify(res, null, 2)}`);
  appendTextConsole(`Metrics: ${JSON.stringify(res.metrics, null, 2)}`);
}

function updateDownloadProgress(data) {
  const downloadsContainer = document.getElementById("downloads");

  const progressPercentage = Math.round(data.progress) || 100;
  let progressBarContainer = document.getElementById(data.id);

  // does not exist, we add it.
  if (!progressBarContainer) {
    // Create a new progress bar container
    progressBarContainer = document.createElement("div");
    progressBarContainer.id = data.id;
    progressBarContainer.className = "progress-bar-container";

    // Create the label
    const label = document.createElement("div");
    label.textContent = data.metadata.file;
    progressBarContainer.appendChild(label);

    // Create the progress bar
    const progressBar = document.createElement("div");
    progressBar.className = "progress-bar";

    const progressBarFill = document.createElement("div");
    progressBarFill.className = "progress-bar-fill";
    progressBarFill.style.width = `${progressPercentage}%`;
    progressBarFill.textContent = `${progressPercentage}%`;
    progressBar.appendChild(progressBarFill);
    progressBarContainer.appendChild(progressBar);
    // Add the progress bar container to the downloads div
    downloadsContainer.appendChild(progressBarContainer);
  } else {
    // Update the existing progress bar
    const progressBarFill =
      progressBarContainer.querySelector(".progress-bar-fill");
    progressBarFill.style.width = `${progressPercentage}%`;
    progressBarFill.textContent = `${progressPercentage}%`;
  }

  if (progressBarContainer && progressPercentage === 100) {
    downloadsContainer.removeChild(progressBarContainer);
  }
}

function appendConsole(data) {
  let text;
  const textarea = document.getElementById("console");
  switch (data.type) {
    case "loading_from_cache":
      text = `Loading ${data.metadata.file} from cache`;
      break;
    case "downloading":
      updateDownloadProgress(data);
      return;
    default:
      text = JSON.stringify(data);
  }
  textarea.value += (textarea.value ? "\n" : "") + text;
}

function appendTextConsole(text) {
  const textarea = document.getElementById("console");
  textarea.value += (textarea.value ? "\n" : "") + text;
}

async function runHttpInference() {
  const output = document.getElementById("http.output");
  output.value = "…";
  output.value = await lazy.HttpInference.completion(
    ["bearer", "endpoint", "model", "prompt"].reduce(
      (config, key) => {
        config[key] = document.getElementById("http." + key).value;
        return config;
      },
      { onStream: val => (output.value = val) }
    ),
    await updateHttpContext()
  );
}

async function updateHttpContext() {
  const limit = document.getElementById("http.limit").value;
  const { AboutNewTab, gBrowser, isBlankPageURL } =
    window.browsingContext.topChromeWindow;
  const recentTabs = gBrowser.tabs
    .filter(
      tab =>
        !isBlankPageURL(tab.linkedBrowser.currentURI.spec) &&
        tab != gBrowser.selectedTab
    )
    .toSorted((a, b) => b.lastSeenActive - a.lastSeenActive)
    .slice(0, limit)
    .map(tab => tab.label);
  const context = {
    recentTabs,
    stories: Object.values(
      AboutNewTab.activityStream.store.getState().DiscoveryStream.feeds.data
    )[0]
      ?.data.recommendations.slice(0, limit)
      .map(rec => rec.title),
    tabTitle: recentTabs[0],
  };

  const output = document.getElementById("http.context");
  output.innerHTML = "";
  const table = output.appendChild(document.createElement("table"));
  Object.entries(context).forEach(([key, val]) => {
    const tr = table.appendChild(document.createElement("tr"));
    tr.appendChild(document.createElement("td")).textContent = `%${key}%`;
    tr.appendChild(document.createElement("td")).textContent = val;
  });

  return context;
}

var selectedHub;
var selectedPreset;

function fillSelect(elementId, values) {
  const selectElement = document.getElementById(elementId);
  values.forEach(function (task) {
    const option = document.createElement("option");
    option.value = task;
    option.text = task;
    selectElement.appendChild(option);
  });
}

/**
 * Initializes the display of information when the window loads and sets an interval to update it.
 *
 * @async
 */
window.onload = async function () {
  await displayInfo();
  loadExample("summary");
  fillSelect("dtype", DTYPE);
  fillSelect("taskName", TASKS);

  document
    .getElementById("inferenceButton")
    .addEventListener("click", runInference);
  document.getElementById("modelHub").addEventListener("change", function () {
    var selectedOption = this.options[this.selectedIndex];
    selectedHub = selectedOption.value;
  });
  document.getElementById("predefined").addEventListener("change", function () {
    var selectedOption = this.options[this.selectedIndex];
    selectedPreset = selectedOption.value;
    loadExample(selectedPreset);
  });

  document
    .getElementById("http.button")
    .addEventListener("click", runHttpInference);
  document
    .getElementById("http.limit")
    .addEventListener("change", updateHttpContext);
  updateHttpContext();
  setInterval(displayInfo, 5000);
};
