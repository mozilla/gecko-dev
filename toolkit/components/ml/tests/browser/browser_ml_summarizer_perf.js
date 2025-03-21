/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const rootDataUrl =
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/data/articles";

async function fetchArticle(url) {
  const response = await fetch(url);
  return await response.text();
}

let testData = [];

const distilBartModel = {
  taskName: "summarization",
  modelId: "Mozilla/distilbart-cnn-12-6",
  dtype: "q8",
  // To keep history, we reuse xenova in the perf name
  perfModelId: "Xenova/distilbart-cnn-12-6",
};

const qwenModel = {
  taskName: "text-generation",
  modelId: "Mozilla/Qwen2.5-0.5B-Instruct",
  dtype: "q8",
  // To keep history, we reuse onnx-community in the perf name
  perfModelId: "onnx-community/Qwen2.5-0.5B-Instruct",
};

const articles = [{ data: `${rootDataUrl}/big.txt`, type: "big" }];

let numEngines = 0;

for (const model of [distilBartModel, qwenModel]) {
  for (const article of articles) {
    // Replace all non-alphabnumeric or dash or underscore by underscore
    const perfName = `${model.perfModelId.replace(/\//g, "-")}_${article.type}`;

    const engineId = `engine-${numEngines}`;

    const options = { ...model, article: article.data, engineId, perfName };

    numEngines += 1;

    options.trackPeakMemory = false;
    testData.push(options);
  }
}

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Summarizer Model",
  description: "Template test for latency for Summarizer model",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "latency",
          unit: "ms",
          shouldAlert: true,
        },
        {
          name: "memory",
          unit: "MiB",
          shouldAlert: true,
        },
        {
          name: "tokenSpeed",
          unit: "tokens/s",
          shouldAlert: true,
          lowerIsBetter: false,
        },
        {
          name: "charactersSpeed",
          unit: "chars/s",
          shouldAlert: true,
          lowerIsBetter: false,
        },
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

requestLongerTimeout(120);

// To run locally
// pip install huggingface-hub
// huggingface-cli download {model_id} --local-dir MOZ_FETCHES_DIR/onnx-models/{model_id}/{revision}

// Update your test in
// Then run:  ./mach lint -l perfdocs --fix .
// This will auto-generate docs
async function run_summarizer_with_perf({
  taskName,
  modelId,
  article,
  dtype,
  engineId,
  perfName,
  trackPeakMemory,
  browserPrefs = null,
}) {
  let chatInput = await fetchArticle(article);

  const minNewTokens = 195;
  const maxNewTokens = 200;

  let requestOptions = {
    max_new_tokens: minNewTokens,
    min_new_tokens: maxNewTokens,
  };

  const options = new PipelineOptions({
    engineId,
    taskName,
    modelHubUrlTemplate: "{model}/{revision}",
    modelId,
    modelRevision: "main",
    dtype,
    useExternalDataFormat: true,
    timeoutMS: -1,
  });

  if (taskName === "text-generation") {
    chatInput = [
      {
        role: "system",
        content:
          "Your role is to summarize the provided content as succinctly as possible while retaining the most important information",
      },
      {
        role: "user",
        content: chatInput,
      },
    ];

    requestOptions = {
      max_new_tokens: minNewTokens,
      min_new_tokens: maxNewTokens,
      return_full_text: true,
      return_tensors: false,
      do_sample: false,
    };
  }

  const request = {
    args: [chatInput],
    options: requestOptions,
  };

  info(`is request null | ${request === null || request === undefined}`);

  await perfTest({
    name: `sum-${perfName}`,
    options,
    request,
    trackPeakMemory,
    browserPrefs,
  });
}

/*
 * distilbart Model
 */
add_task(async function test_ml_distilbart_tiny_article() {
  await run_summarizer_with_perf(testData[0]);
});

add_task(async function test_ml_distilbart_tiny_article_mem() {
  await run_summarizer_with_perf({ ...testData[0], trackPeakMemory: true });
});

add_task(async function test_ml_distilbart_tiny_article_mem_no_ion() {
  await run_summarizer_with_perf({
    ...testData[0],
    trackPeakMemory: true,
    browserPrefs: [["javascript.options.wasm_optimizingjit", false]],
  });
});

/*
 * Qwen model
 */
add_task(async function test_ml_qwen_big_article() {
  await run_summarizer_with_perf(testData[1]);
});

add_task(async function test_ml_qwen_big_article_with_mem() {
  await run_summarizer_with_perf({ ...testData[1], trackPeakMemory: true });
});
