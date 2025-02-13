/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// 1024 tokens
const bigArticle =
  "The Transformative Power of Traveling: Exploring the World and Ourselves\nTraveling is much more than moving from one location to another. It’s a transformative journey that reshapes perspectives, deepens understanding, and rejuvenates the spirit. Whether it's a weekend road trip, a months-long backpacking adventure, or an exploration of cultural hubs across the globe, traveling has the potential to profoundly impact the way we view the world and ourselves.\n\nThe Thrill of Discovery\nAt its core, traveling is about discovery. For some, it’s the thrill of stepping into a bustling city like Tokyo, Paris, or Istanbul, where every street offers something new: food, art, and traditions woven into daily life. For others, it's the serenity of nature that beckons — trekking through the Himalayas, wandering across the Saharas golden dunes, or diving into the vibrant coral reefs of the Great Barrier Reef. Each journey unravels layers of wonder, revealing a world far more diverse and intricate than we imagine.\n\nBut it’s not just external landscapes we explore; traveling also encourages inner discovery. When we step out of our comfort zones, we confront unfamiliar situations that test resilience, adaptability, and creativity. Lost in an unfamiliar city without Wi-Fi? You might find yourself relying on the kindness of strangers or your resourcefulness. A delayed flight or a sudden change in plans? These moments teach patience and the art of letting go.\n\nBridging Cultures and Building Connections\nTraveling allows us to bridge gaps between cultures, fostering a deeper understanding of humanity's shared values. When we interact with people from different walks of life, we begin to appreciate their customs, beliefs, and way of living. Sharing meals with locals, participating in traditional festivals, or simply engaging in conversation over coffee broadens horizons and builds empathy.\n\nConsider a traveler visiting Morocco, where the vibrant souks of Marrakech provide a sensory overload of spices, textiles, and handmade crafts. Beyond the market’s hustle, they might join a family for a traditional tagine dinner, learning not just about the dish but also about Moroccan hospitality. These moments linger in memory, reminding us of our shared human experience, no matter where we’re from.\n\nLanguage barriers often turn into opportunities for connection. A smile, a gesture, or a shared laugh transcends words. These interactions remind us that kindness and curiosity are universal languages, opening doors to friendships and enriching our lives.\n\nEscaping the Routine\nIn our fast-paced world, filled with deadlines, notifications, and responsibilities, traveling offers a much-needed escape from the daily grind. It disrupts monotony, encouraging a sense of spontaneity and adventure. Whether it’s waking up to the call of exotic birds in a rainforest or watching the sun set over a tranquil beach, these moments ground us, reminding us of life’s simple yet profound joys.\n\nThe journey itself becomes an opportunity to practice mindfulness. Without the usual demands of work or routines, travelers can immerse themselves in the present moment. Sipping coffee in a quaint Italian piazza, marveling at the Northern Lights in Iceland, or walking through Kyoto’s serene bamboo forests invites us to slow down and savor life.\n\nTraveling Responsibly\nAs we venture into the world, it’s crucial to acknowledge our impact on the places we visit. Overtourism, environmental degradation, and cultural exploitation are real challenges. Traveling responsibly means making choices that respect local communities, preserve natural resources, and minimize harm.\n\nOpting for eco-friendly accommodations, supporting local businesses, and learning about the cultural etiquette of a destination are small but impactful steps. Traveling with intention — valuing quality experiences over quantity — ensures that we leave a positive footprint. It's about giving back to the places that give so much to us.\n\nThe Lessons We Bring Home\nPerhaps the most remarkable thing about traveling is the way it changes us. Every trip, no matter how short or long, leaves a mark. We return home not just with souvenirs but with stories, insights, and often a renewed sense of gratitude.\n\nTraveling teaches humility. Standing before the vastness of the Grand Canyon or the towering spires of a medieval cathedral reminds us of our small place in the grand scheme of things. It also instills confidence, as navigating foreign streets or overcoming travel hiccups proves our capability.\n\nFinally, traveling ignites a sense of curiosity that lingers long after the journey ends. It encourages us to keep exploring, whether it's the next destination on our bucket list or the unexplored corners of our own neighborhoods. This mindset of discovery enriches everyday life, reminding us that the world is filled with opportunities for awe.\n\nConclusion\nTraveling is not just a luxury or a pastime; it's an investment in personal growth and global understanding. It challenges us to step outside our routines, confront the unfamiliar, and embrace the beauty of diversity. In doing so, we find not only the magic of the world but also the boundless potential within ourselves.\n\nSo pack your bags, set out with an open heart, and let the world teach you. Because, as the saying goes, Travel is the only thing you buy that makes you richer.";

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

const articles = [{ data: bigArticle, type: "big" }];

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
  let chatInput = article;

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
        content: article,
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
