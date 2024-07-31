Machine Learning
================

This component is an experimental machine learning local inference engine based on
`Transformers.js <https://huggingface.co/docs/transformers.js/index>`_ and
the `ONNX runtime <https://onnxruntime.ai/>`_.

You can use the component to leverage the inference runtime in the context
of the browser. To try out some inference tasks, you can refer to the
`1000+ models <https://huggingface.co/models?library=transformers.js>`_
that are available in the Hugging Face Hub that are compatible with this runtime.

To enable the engine, flip the `browser.ml.enable` preference to `true` in `about:config`.

Running the pipeline API
::::::::::::::::::::::::

You can use the Transformer.js `pipeline` API directly to perform inference, as long
as the model is in our model hub.

The `Transformers.js documentation <https://huggingface.co/tasks>`_ provides a lot
of examples that you can slightly adapt when running in Firefox.

In the example below, a text summarization task is performed using the `summarization` task:

.. code-block:: javascript

  const { createEngine } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");
  const options = {
    taskName: "summarization",
    modelId: "mozilla/text_summarization",
    modelRevision: "main"
    }
  );

  const engine = await createEngine(options);

  const text = 'The tower is 324 metres (1,063 ft) tall, about the same height as an 81-storey building, ' +
  'and the tallest structure in Paris. Its base is square, measuring 125 metres (410 ft) on each side. ' +
  'During its construction, the Eiffel Tower surpassed the Washington Monument to become the tallest ' +
  'man-made structure in the world, a title it held for 41 years until the Chrysler Building in New ' +
  'York City was finished in 1930. It was the first structure to reach a height of 300 metres. Due to ' +
  'the addition of a broadcasting aerial at the top of the tower in 1957, it is now taller than the ' +
  'Chrysler Building by 5.2 metres (17 ft). Excluding transmitters, the Eiffel Tower is the second ' +
  'tallest free-standing structure in France after the Millau Viaduct.';

  const request = { args:  [text], options: { max_length: 100 } };
  const res = await engine.run(request);
  console.log(res[0]["summary_text"]);


When running this code, Firefox will look for models in the Mozilla model hub located at https://model-hub.mozilla.org
which contains a curated list of models.

Available Options
:::::::::::::::::

Options passed to the `createEngine` function are verified and converted into a `PipelineOptions` object.

Below are the options available:

- **taskName**: The name of the task the pipeline is configured for.
- **engineId**:  The identifier for the engine to be used by the pipeline.
- **timeoutMS**: The maximum amount of time in milliseconds the pipeline should wait for a response.
- **modelHubRootUrl**: The root URL of the model hub where models are hosted.
- **modelHubUrlTemplate**: A template URL for building the full URL for the model.
- **modelId**: The identifier for the specific model to be used by the pipeline.
- **modelRevision**: The revision for the specific model to be used by the pipeline.
- **tokenizerId**: The identifier for the tokenizer associated with the model, used for pre-processing inputs.
- **tokenizerRevision**: The revision for the tokenizer associated with the model, used for pre-processing inputs.
- **processorId**: The identifier for any processor required by the model, used for additional input processing.
- **processorRevision**: The revision for any processor required by the model, used for additional input processing.
- **logLevel**: The log level used in the worker
- **runtimeFilename**: Name of the runtime wasm file.

**taskName** and **modelId** are required, the others are optional and will be filled automatically
using values pulled from Remote Settings when the task id is recognized.

**engineId** is used to manage the lifecycle of the engine. When not provided, it defaults to
`default-engine`. Everytime a new engine is created using `createEngine` the API will ensure that
there's a single engine with the given id. If the options of the existing engine are not different,
the instance is reused. If they differ, the engine is reinitialized with the new options.
This ensures we don't have too many engines running at once since it takes a lot of resources.
To make sure your engine is not destroyed or reused elsewhere, set that value with a unique id
that matches your component.

Some values are also set from the preferences (set in `about:config`):

- browser.ml.logLevel
- browser.ml.modelHubRootUrl
- browser.ml.modelHubUrlTemplate
- browser.ml.modelCacheTimeout


Using the Hugging Face model hub
::::::::::::::::::::::::::::::::

By default, the engine will use the Mozilla model hub and will error out if you try to use any other hub for security reasons.

If you want to use the Hugging Face model hub, you will need to run Firefox with the `MOZ_ALLOW_EXTERNAL_ML_HUB` environment variable
set to `1`, then set in `about:config` these two values:

- `browser.ml.modelHubRootUrl` to `https://huggingface.co`
- `browser.ml.modelHubUrlTemplate` to `{model}/resolve/{revision}`

The inference engine will then look for models in the Hugging Face model hub.


Running the internal APIs
:::::::::::::::::::::::::

Some inference tasks are doing more complex operations within the engine, such as image processing.
For these tasks, you can use the internal APIs to run the inference. Those tasks are prefixed with `moz`.

In the example below, an image is converted to text using the `moz-image-to-text` task.


.. code-block:: javascript

  const { createEngine } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

  // options needed for the task
  const options = {taskName: "moz-image-to-text" };

  // We create the engine object, using the options
  const engine = await createEngine(options);

  // Preparing a request
  const request = {url: "https://huggingface.co/datasets/mishig/sample_images/resolve/main/football-match.jpg"};

  // At this point we are ready to do some inference.
  const res = await engine.run(request);

  // The result is a string containing the text extracted from the image
  console.log(res);


The following internal tasks are supported by the machine learning engine:

.. js:autofunction:: imageToText


Notifications callback
::::::::::::::::::::::

When initializing or running the engine, certain operations may take considerable time to complete.
You can receive progress notifications for these operations using a callback function.

Currently, progress notifications are supported only for model downloads.
When the engine is created, it will download any model not already in the cache.

Below is an example of using the callback function with the image-to-text model:

.. code-block:: javascript

  const { createEngine } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

  // options needed for the task
  const options = {taskName: "moz-image-to-text" };

  // We create the engine object, using options and a callback
  const engine = await createEngine(options, progressData => {
    console.log("Received progress data", progressData);
  });


In the code above, **progressData** is an object of type `ProgressAndStatusCallbackParams` containing the following fields:

- **progress**: A float indicating the percentage of data loaded. Note that 100% does not necessarily mean the operation is complete.
- **totalLoaded**: A float indicating the total amount of data loaded so far.
- **currentLoaded**: The amount of data loaded in the current callback call.
- **total**: A float indicating an estimate of the total amount of data to be loaded.
- **units**: The units in which the amounts are reported.
- **type**: The name of the operation being tracked. It will be one of `ProgressType.DOWNLOAD`, `ProgressType.LOAD_FROM_CACHE`.
- **statusText**: A message indicating the status of the tracked operation, which can be:

  - `ProgressStatusText.INITIATE` Indicates that an operation has started. This will be used exactly once for each operation uniquely identified by `id` and `type`.

  - `ProgressStatusText.SIZE_ESTIMATE` Indicates an estimate for the size of the operation. This will be used exactly once for each operation uniquely identified by `id` and `type`, updating the `total`` field.

  - `ProgressStatusText.IN_PROGRESS` Indicates that an operation is in progress. This will be used each time progress occurs, updating the `totalLoaded`` and `currentLoaded`` fields.

  - `ProgressStatusText.DONE`  indicating that an operation has completed.

- **id**: An ID uniquely identifying the object/file being tracked.
- **ok**: A boolean indicating if the operation was succesfull.
- **metadata**: Any additional metadata for the operation being tracked.


about:inference
:::::::::::::::

In Nightly, you can use `about:inference <about:inference>`_ to check the status of the engine and manage downloads of models.
