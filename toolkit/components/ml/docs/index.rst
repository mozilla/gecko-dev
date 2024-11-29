Firefox AI Platform
===================

This component is an experimental machine learning local inference engine based on
`Transformers.js <https://huggingface.co/docs/transformers.js/index>`_ and
the `ONNX runtime <https://onnxruntime.ai/>`_. You can use the component to leverage
the inference runtime in the context of the browser. To try out some inference tasks,
you can refer to the
`1000+ models <https://huggingface.co/models?library=transformers.js>`_
that are available in the Hugging Face Hub that are compatible with this runtime.

To enable the engine, flip the `browser.ml.enable` preference to `true` in `about:config`
then visit **about:inference** (Nightly only) or add the following snippet of code
into your (privileged) Javascript code in Firefox or in the browser console:

.. code-block:: javascript

  const { createEngine } = ChromeUtils.importESModule("chrome://global/content/ml/EngineProcess.sys.mjs");
  const engine = await createEngine({taskName: "summarization"});
  const request = { args:  ["This is the text to summarize"]};
  const res = await engine.run(request);
  console.log(res[0]["summary_text"]);


Learn more about the platform:

.. toctree::
   :maxdepth: 1

   architecture
   api
   notifications
   models
   perf
   extensions
   extensions-api-example/README
