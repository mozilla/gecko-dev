/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const PREFIX = "inference";
const METRICS = [
  `${PREFIX}-${PIPELINE_READY_LATENCY}`,
  `${PREFIX}-${INITIALIZATION_LATENCY}`,
  `${PREFIX}-${MODEL_RUN_LATENCY}`,
  `${PREFIX}-${TOTAL_MEMORY_USAGE}`,
];
const journal = {};
for (let metric of METRICS) {
  journal[metric] = [1];
}

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Suggest Inference Model",
  description: "Template test for ML suggest Inference Model",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "inference-pipeline-ready-latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "inference-initialization-latency",
          unit: "ms",
          shouldAlert: true,
        },
        { name: "inference-model-run-latency", unit: "ms", shouldAlert: true },
        { name: "inference-total-memory-usage", unit: "ms", shouldAlert: true },
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

requestLongerTimeout(120);

const CUSTOM_INTENT_OPTIONS = {
  taskName: "text-classification",
  featureId: "suggest-intent-classification",
  engineId: "ml-suggest-intent",
  timeoutMS: -1,
  modelId: "Mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier",
  dtype: "q8",
  modelRevision: "main",
};

const CUSTOM_NER_OPTIONS = {
  taskName: "token-classification",
  featureId: "suggest-NER",
  engineId: "ml-suggest-ner",
  timeoutMS: -1,
  modelId: "Mozilla/distilbert-uncased-NER-LoRA",
  dtype: "q8",
  modelRevision: "main",
};

const ROOT_URL =
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data/suggest";
const YELP_KEYWORDS_DATA = `${ROOT_URL}/yelp_val_keywords_data.json`;
const YELP_VAL_DATA = `${ROOT_URL}/yelp_val_generated_data.json`;
const NER_VAL_DATA = `${ROOT_URL}/named_entity_val_generated_data.json`;

async function get_val_data(inputDataPath) {
  const response = await fetch(inputDataPath);
  if (!response.ok) {
    throw new Error(
      `Failed to fetch data: ${response.statusText} from ${inputDataPath}`
    );
  }
  return response.json();
}

async function read_data_by_type(type) {
  let data;
  if (type === "YELP_KEYWORDS_DATA") {
    data = await get_val_data(YELP_KEYWORDS_DATA);
    return data[0].subjects;
  } else if (type === "YELP_VAL_DATA") {
    data = await get_val_data(YELP_VAL_DATA);
    return data.queries;
  } else if (type === "NER_VAL_DATA") {
    data = await get_val_data(NER_VAL_DATA);
    return data.queries;
  }
  return [];
}

// Utility to write results to a local JSON file using IOUtils
async function writeResultsToFile(results, type) {
  try {
    const json = JSON.stringify(results, null, 2);
    const OUTPUT_FILE_PATH = normalizePathForOS(
      `${
        Services.dirsvc.get("DfltDwnld", Ci.nsIFile).path
      }/ML_output_${type}.json`
    );
    await IOUtils.writeUTF8(OUTPUT_FILE_PATH, json);
    console.log("Results successfully written to:", OUTPUT_FILE_PATH);
  } catch (error) {
    console.error("Failed to write results to file:", error);
    Assert.ok(false, "Failed to write results to file");
  }
}

async function perform_inference(queries, type) {
  // Ensure MLSuggest is initialized
  await MLSuggest.initialize();

  const batchSize = 32;
  const results = [];

  // Process in batches of 32
  for (let i = 0; i < queries.length; i += batchSize) {
    const batchQueries = queries.slice(i, i + batchSize);
    const batchResults = await Promise.all(
      batchQueries.map(async query => {
        const suggestion = await MLSuggest.makeSuggestions(query);
        const res = {
          query,
          intent: suggestion.intent,
          city: suggestion.location.city,
          state: suggestion.location.state,
        };
        return res;
      })
    );
    results.push(...batchResults);
  }
  Assert.ok(
    results.length === queries.length,
    "results size should be equal to queries size."
  );
  // Write results to a file
  await writeResultsToFile(results, type);
}

const runInference2 = async () => {
  ChromeUtils.defineESModuleGetters(this, {
    MLSuggest: "resource:///modules/urlbar/private/MLSuggest.sys.mjs",
  });

  // Override INTENT and NER options within MLSuggest
  MLSuggest.INTENT_OPTIONS = CUSTOM_INTENT_OPTIONS;
  MLSuggest.NER_OPTIONS = CUSTOM_NER_OPTIONS;

  const modelDirectory = normalizePathForOS(
    `${Services.env.get("MOZ_FETCHES_DIR")}/onnx-models`
  );
  info(`Model Directory: ${modelDirectory}`);
  const { baseUrl: modelHubRootUrl } = startHttpServer(modelDirectory);
  info(`ModelHubRootUrl: ${modelHubRootUrl}`);
  const { cleanup } = await perfSetup({
    prefs: [
      ["browser.ml.modelHubRootUrl", modelHubRootUrl],
      ["javascript.options.wasm_lazy_tiering", true],
    ],
  });

  const TYPES = ["YELP_KEYWORDS_DATA", "YELP_VAL_DATA", "NER_VAL_DATA"];
  for (const type of TYPES) {
    info(`processing ${type} now`);
    // Read data for the current type
    const queries = await read_data_by_type(type);
    if (!queries) {
      info(`No queries found for type: ${type}`);
      continue;
    }
    // Run inference for each query
    await perform_inference(queries, type);
  }
  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();
  await cleanup();
};

/**
 * Tests remote ml model
 */
add_task(async function test_ml_generic_pipeline() {
  await runInference2();
  reportMetrics(journal);
});
