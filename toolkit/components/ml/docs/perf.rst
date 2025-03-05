How to perftest a model
=======================

For each model running inside Firefox, we want to determine its performance
in terms of speed and memory usage and track it over time.

To do so, we use the `Perfherder <https://wiki.mozilla.org/Perfherder>`_ infrastructure
to gather the performance metrics.

Adding a new performance test is done in two steps:
1. making it work locally
2. add it in perfherder

Run locally
-----------

To test the performance of a model, you can add in the `tests/browser` a new file
with the following structure and adapt it to your needs:

.. code-block:: javascript

  "use strict";

  // unfortunately we have to write a full static structure here
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1930955
  const perfMetadata = {
    owner: "GenAI Team",
    name: "ML Test Model",
    description: "Template test for latency for ml models",
    options: {
      default: {
        perfherder: true,
        perfherder_metrics: [
          { name: "pipeline-ready-latency", unit: "ms", shouldAlert: true },
          { name: "initialization-latency", unit: "ms", shouldAlert: true },
          { name: "model-run-latency", unit: "ms", shouldAlert: true },
          { name: "pipeline-ready-memory", unit: "MB", shouldAlert: true },
          { name: "initialization-memory", unit: "MB", shouldAlert: true },
          { name: "model-run-memory", unit: "MB", shouldAlert: true },
          { name: "total-memory-usage", unit: "MB", shouldAlert: true },
        ],
        verbose: true,
        manifest: "perftest.toml",
        manifest_flavor: "browser-chrome",
        try_platform: ["linux", "mac", "win"],
      },
    },
  };

  requestLongerTimeout(120);

  add_task(async function test_ml_generic_pipeline() {
    const options = {
      taskName: "feature-extraction",
      modelId: "Xenova/all-MiniLM-L6-v2",
      modelHubUrlTemplate: "{model}/{revision}",
      modelRevision: "main",
    };

    const args = ["The quick brown fox jumps over the lazy dog."];
    await perfTest("example", options, args);
  });


Then add the file in `perftest.toml` and rebuild with `./mach build`.

The test downloads models it uses from the local disk, so you need to prepare them.

We provide a script to automate this.

.. code-block:: bash

  $ mach python toolkit/components/ml/tests/tools/create_local_hub.py --list-models
  Available git-based models from the YAML:

  - xenova-all-minilm-l6-v2 -> path-prefix: onnx-models/Xenova/all-MiniLM-L6-v2/main/
  - mozilla-ner -> path-prefix: onnx-models/Mozilla/distilbert-uncased-NER-LoRA/main/
  - mozilla-intent -> path-prefix: onnx-models/Mozilla/mobilebert-uncased-finetuned-LoRA-intent-classifier/main/
  - mozilla-autofill -> path-prefix: onnx-models/Mozilla/tinybert-uncased-autofill/main/
  - distilbart-cnn-12-6 -> path-prefix: onnx-models/Mozilla/distilbart-cnn-12-6/main/
  - qwen2.5-0.5-instruct -> path-prefix: onnx-models/Mozilla/Qwen2.5-0.5B-Instruct/main/
  - mozilla-smart-tab-topic -> path-prefix: onnx-models/Mozilla/smart-tab-topic/main/
  - mozilla-smart-tab-emb -> path-prefix: onnx-models/Mozilla/smart-tab-embedding/main/

  (Use `--model <key>` to clone one of these repositories.)

You can then use `--model` to download locally models, by specifying the local
`MOZ_FETCHES_DIR` directory, via the env var or command line argument :

.. code-block:: bash

  $ mach python toolkit/components/ml/tests/tools/create_local_hub.py --model mozilla-smart-tab-emb --fetches-dir ~/ml-fetches
  Found existing file /Users/tarekziade/Dev/fetches/ort-wasm-simd-threaded.jsep.wasm, verifying checksum...
  Existing file's checksum matches. Skipping download.
  Updated Git hooks.
  Git LFS initialized.
  Cloning https://huggingface.co/Mozilla/smart-tab-embedding into '/Users/tarekziade/Dev/fetches/onnx-models/Mozilla/smart-tab-embedding/main...
  Cloning in '/Users/tarekziade/Dev/fetches/onnx-models/Mozilla/smart-tab-embedding/main'...
  Checked out revision '2278e76f67ada584cfd3149fd2661dad03674e4d' in '/Users/tarekziade/Dev/fetches/onnx-models/Mozilla/smart-tab-embedding/main'.

Once done, you should then be able to run it locally with :

.. code-block:: bash

   $ MOZ_FETCHES_DIR=~/ml-fetches ./mach perftest toolkit/components/ml/tests/browser/browser_ml_engine_perf.js --mochitest-extra-args=headless

Notice that `MOZ_FETCHES_DIR` is an absolute path to the `root` directory.

Add in the CI
-------------

To add the test in the CI you need to add an entry in

- `taskcluster/kinds/perftest/linux.yml`
- `taskcluster/kinds/perftest/windows11.yml`
- `taskcluster/kinds/perftest/macos.yml`

With a unique name that starts with `ml-perf`

Example for Linux:

.. code-block:: yaml

  ml-perf:
      fetches:
          fetch:
              - ort.wasm
              - ort.jsep.wasm
              - ort-training.wasm
              - xenova-all-minilm-l6-v2
      description: Run ML Models Perf Tests
      treeherder:
          symbol: perftest(linux-ml-perf)
          tier: 2
      attributes:
          batch: false
          cron: false
      run-on-projects: [autoland, mozilla-central]
      run:
          command: >-
              mkdir -p $MOZ_FETCHES_DIR/../artifacts &&
              cd $MOZ_FETCHES_DIR &&
              python3 python/mozperftest/mozperftest/runner.py
              --mochitest-binary ${MOZ_FETCHES_DIR}/firefox/firefox-bin
              --flavor mochitest
              --output $MOZ_FETCHES_DIR/../artifacts
              toolkit/components/ml/tests/browser/browser_ml_engine_perf.js

You also need to add the models your test uses (like the ones you've downloaded locally) by adding entries in
`taskcluster/kinds/fetch/onnxruntime-web-fetch.yaml` and adapting the `fetches` section.


Once this is done, try it out with:

.. code-block:: bash

   $ ./mach try perf --single-run --full --artifact


You should then see the results in treeherder.
