/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This metadata schema is parsed by the perftest infrastructure.
 *
 * The perftest runner then scrapes the logs for a JSON results matching this schema,
 * which are logged by the TranslationsBencher class.
 *
 * @see {TranslationsBencher.Journal}
 */
const perfMetadata = {
  owner: "Translations Team",
  name: "Full-Page Translation (Spanish to English)",
  description:
    "Tests the speed of Full Page Translations using the Spanish-to-English model.",
  options: {
    default: {
      perfherder: true,
      perfherder_metrics: [
        {
          name: "engine-init-time",
          unit: "ms",
          shouldAlert: true,
          lowerIsBetter: true,
        },
        {
          name: "words-per-second",
          unit: "WPS",
          shouldAlert: true,
          lowerIsBetter: false,
        },
        {
          name: "tokens-per-second",
          unit: "TPS",
          shouldAlert: true,
          lowerIsBetter: false,
        },
        {
          name: "total-memory-usage",
          unit: "MiB",
          shouldAlert: true,
          lowerIsBetter: true,
        },
        {
          name: "total-translation-time",
          unit: "s",
          shouldAlert: true,
          lowerIsBetter: true,
        },
      ],
      verbose: true,
      manifest: "perftest.toml",
      manifest_flavor: "browser-chrome",
      try_platform: ["linux", "mac", "win"],
    },
  },
};

/**
 * Request 2x longer timeout for this test.
 */
requestLongerTimeout(2);

/**
 * Runs the translations benchmark tests from Spanish to English.
 */
add_task(async function test_translations_performance_es_en() {
  await TranslationsBencher.benchmarkTranslation({
    page: SPANISH_BENCHMARK_PAGE_URL,
    runCount: 5,
    sourceLanguage: "es",
    targetLanguage: "en",
  });
});
