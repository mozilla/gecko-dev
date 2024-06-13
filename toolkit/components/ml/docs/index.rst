Machine Learning
================

This component is an experimental machine learning local inference engine based on
Transformers.js and the ONNX runtime.

You can use the Transformer.js `pipeline` API directly to perform inference, as long
as the model is in our model hub.

In the example below, a text summarization task is performed using the `summarization` task:

.. code-block:: javascript

  const {PipelineOptions, EngineProcess } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");
  const options = new PipelineOptions(
    {
    taskName: "summarization",
    modelId: "mozilla/text_summarization",
    modelRevision: "main"
    }
  );

  const engineParent = await EngineProcess.getMLEngineParent();
  const engine = engineParent.getEngine(options);

  const text = 'The tower is 324 metres (1,063 ft) tall, about the same height as an 81-storey building, ' +
  'and the tallest structure in Paris. Its base is square, measuring 125 metres (410 ft) on each side. ' +
  'During its construction, the Eiffel Tower surpassed the Washington Monument to become the tallest ' +
  'man-made structure in the world, a title it held for 41 years until the Chrysler Building in New ' +
  'York City was finished in 1930. It was the first structure to reach a height of 300 metres. Due to ' +
  'the addition of a broadcasting aerial at the top of the tower in 1957, it is now taller than the ' +
  'Chrysler Building by 5.2 metres (17 ft). Excluding transmitters, the Eiffel Tower is the second ' +
  'tallest free-standing structure in France after the Millau Viaduct.';

  const request = { data:  text, options: { max_length: 100 } };
  const res = await engine.run(request);
  console.log(res[0]["summary_text"]);


In the example below, an image is converted to text using the `image-to-text` task.


.. code-block:: javascript

  const {PipelineOptions, EngineProcess } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");

  // First we create a pipeline options object, which contains the task name
  // and any other options needed for the task
  const options = new PipelineOptions({taskName: "moz-image-to-text" });

  // Next, we create an engine parent object via EngineProcess
  const engineParent = await EngineProcess.getMLEngineParent();

  // We then create the engine object, using the options
  const engine = engineParent.getEngine(options);

  // Preparing a request
  const request = {url: "https://huggingface.co/datasets/mishig/sample_images/resolve/main/football-match.jpg"};

  // At this point we are ready to do some inference.
  const res = await engine.run(request);

  // The result is a string containing the text extracted from the image
  console.log(res);


Supported Inference Tasks
:::::::::::::::::::::::::

The following tasks are supported by the machine learning engine:

.. js:autofunction:: imageToText
