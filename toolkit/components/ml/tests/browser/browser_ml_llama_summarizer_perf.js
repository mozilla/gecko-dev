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

const smollm2Model = {
  taskName: "text-generation",
  modelId: "HuggingFaceTB/SmolLM2-360M-Instruct-GGUF",
  modelFile: "smollm2-360m-instruct-q8_0.gguf",
  kvCacheDtype: "q8_0",
  flashAttn: true,
  useMmap: true,
  useMlock: false,
  perfModelId: "HuggingFaceTB/SmolLM2-360M-Instruct",
};

const articles = [
  {
    data: `${rootDataUrl}/tiny.txt`,
    type: "tiny",
    numTokens: 200,
  },
  {
    data: `${rootDataUrl}/medium.txt`,
    type: "medium",
    numTokens: 568,
  },
];

let numEngines = 0;

for (const model of [smollm2Model]) {
  for (const article of articles) {
    // Replace all non-alphabnumeric or dash or underscore by underscore
    const perfName = `${model.perfModelId.replace(/\//g, "-")}_${article.type}`;

    const engineId = `engine-${numEngines}`;

    const options = {
      ...model,
      article: article.data,
      engineId,
      perfName,
      numTokens: article.numTokens,
    };

    numEngines += 1;

    testData.push(options);
  }
}

const perfMetadata = {
  owner: "GenAI Team",
  name: "ML Llama Summarizer Model",
  description:
    "Template test for latency for Summarizer model using Llama.cpp WASM",
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
          unit: "MB",
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
  engineId,
  perfName,
  numTokens,
  trackPeakMemory,
  ...llamaOptions
}) {
  let chatInput = await fetchArticle(article);

  const numNewTokens = 80;

  const nextPowerOf2 = n => {
    if (n <= 1) {
      return 1;
    }
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
  };

  const numContext = nextPowerOf2(numTokens + numNewTokens + 10);

  const options = new PipelineOptions({
    engineId,
    taskName,
    modelHubUrlTemplate: "{model}/{revision}",
    modelId,
    modelRevision: "main",
    numContext,
    numBatch: Math.min(numContext, 64),
    numUbatch: Math.min(numContext, 64),
    backend: "wllama",
    timeoutMS: -1,
    ...llamaOptions,
  });

  if (taskName.includes("text-generation")) {
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
  }

  const request = {
    prompt: chatInput,
    nPredict: numNewTokens,
    skipPrompt: false,
  };

  await perfTest({
    name: `sum-${perfName}`,
    options,
    request,
    trackPeakMemory,
  });
}

add_task(async function test_ml_smollm_tiny_article() {
  await run_summarizer_with_perf(testData[0]);
});

add_task(async function test_ml_smollm_medium_article() {
  await run_summarizer_with_perf(testData[1]);
});

add_task(async function test_ml_smollm_tiny_article_with_mem() {
  await run_summarizer_with_perf({ ...testData[0], trackPeakMemory: true });
});

add_task(async function test_ml_smollm_medium_article_with_mem() {
  await run_summarizer_with_perf({ ...testData[1], trackPeakMemory: true });
});
