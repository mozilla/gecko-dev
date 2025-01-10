Models management
=================


Prepare a model for Firefox
:::::::::::::::::::::::::::

Models that can be used with Firefox should have ONNX weights at different quantization levels.

In order to make sure we are compatible with Transformers.js, we use the conversion script
provided by that project, which checks that the model arhitecture will work and has
been tested.

To do this, follow these steps:

- make sure your model is published in Hugging Face with PyTorch or SafeTensor weights.
- clone https://github.com/xenova/transformers.js and checkout branch `v3`
- go into `scripts/`
- create a virtualenv there and install requirements from the local `requirements.txt` file

Then you can run:

.. code-block:: bash

  python convert.py --model_id organizationId/modelId --quantize --modes fp16 q8 q4 --task the-inference-task

You will get a new directory in `models/organizationId/modelId` that includes an `onnx` directory and
other files. Upload everything into Hugging Face.

Congratulations! you have a Firefox-compatible model. You can now try it in `about:inference`.

Notice that for the encoder-decoder models with two files, you may need to rename `decoder_model_quantized.onnx`
to `decoder_model_merged_quantized.onnx`, and make similar changes to the fp16, q4 versions.
You do not need to rename the encoder models.

By default, the conversion script above generates a single file containing both the ONNX model architecture and its weights.

To split the model architecture and weights into separate files, you can use the script provided at:
`convert_to_external_data.py <https://searchfox.org/mozilla-central/source/toolkit/components/ml/tools/convert_to_external_data.py>`_.

This process, known as using the external data format, provides additional speed and memory benefits for your model.

For large models, this step is essential, as ONNX files have a 2GB size limit.
Without splitting the model into multiple files, it would be impossible to run models that exceed or are close to the 2GB limit.
Using the external data format ensures compatibility and allows such models to run successfully.


Lifecycle
:::::::::

When Firefox uses a model, it will

1. read metadata stored in Remote Settings
2. download model files from our hub
3. store the files in IndexDB

.. _inference-remote-settings:

1. Remote Settings
------------------

We have two collections in Remote Settings:

- `ml-onnx-runtime`: provides all the WASM files we need to run the inference platform.
- `ml-inference-options`: provides for each `taskId` a list of running options, such as the `modelId`.

Running the inference API will download the WASM files if needed, and then see
if there's an entry for the task in `ml-inference-options`, to grab the options.
That allows us to set the default running options for each task.

This is also how we can update a model without changing Firefox's code:
setting a new revision for a model in Remote Settings will trigger a new download for our users.

Records in `ml-inference-options` are uniquely identified by `featureId`. When not provided,
it falls back to `taskName`. This collection will provide all the options required for that
feature.

For example, the PDF.js image-to-text record is:

.. code-block:: json

   {
   "featureId": "pdfjs-alt-text"
   "dtype":"q8",
   "modelId":"mozilla/distilvit",
   "taskName":"image-to-text",
   "processorId":"mozilla/distilvit",
   "tokenizerId":"mozilla/distilvit",
   "modelRevision":"v0.5.0",
   "processorRevision":"v0.5.0"
   }


If you are adding in Firefox a new inference call, create a new unique `featureId` in `FEATURES <https://searchfox.org/mozilla-central/source/toolkit/components/ml/content/EngineProcess.sys.mjs>`_ and add a record in `ml-inference-options` with the task settings.

By doing this, you will be able to create an engine with this simple call:

.. code-block:: javascript

  const engine = await createEngine({featureId: "pdfjs-alt-text"});



2. Model Hub
------------

Our Model hub follows the same structure than Hugging Face, each file for a model is under
a unique URL:

  `https://model-hub.mozilla.org/<organization>/<model>/<revision>/<path>`

Where:
- `organization` and `name` are the model id. example " `mozilla/distivit`"
- `revision` is the branch or version
- `path` is the path to the file.


Model files downloaded from the hub are stored in IndexDB so users don't need to download them again.

Model files
:::::::::::

Models consists of several files like its configuration, tokenizer, training metadata, and weights.

Below are the most common files you’ll encounter:

1. Model Weights
----------------

- ``pytorch_model.bin``: Contains the model's weights for PyTorch models. It is a serialized file that holds the parameters of the neural network.
- ``tf_model.h5``: TensorFlow's version of the model weights.
- ``flax_model.msgpack``: For models built with the Flax framework, this file contains the model weights in a format used by JAX and Flax.
- ``onnx``: A subdirectory containing ONNX weights files in different quantization levels. **They are the one our platform uses**


2. Model Configuration
----------------------

The ``config.json`` file contains all the necessary configurations for the model architecture,
such as the number of layers, hidden units, attention heads, activation functions, and more.
This allows the Hugging Face library to reconstruct the model exactly as it was defined.

3. Tokenizer Files
------------------

- ``vocab.txt`` or ``vocab.json``: Vocabulary files that map tokens (words, subwords, or characters) to IDs. Different tokenizers (BERT, GPT-2, etc.) will have different formats.
- ``tokenizer.json``: Stores the full tokenizer configuration and mappings.
- ``tokenizer_config.json``: This file contains settings that are specific to the tokenizer used by the model, such as whether it is case-sensitive or the special tokens it uses (e.g., [CLS], [SEP], etc.).

4. Preprocessing Files
----------------------

- ``special_tokens_map.json``: Maps the special tokens (like padding, CLS, SEP, etc.) to the token IDs used by the tokenizer.
- ``added_tokens.json``: If any additional tokens were added beyond the original vocabulary (like custom tokens or domain-specific tokens), they are stored in this file.

5. Training Metadata
--------------------
- ``training_args.bin``: Contains the arguments that were used during training, such as learning rates, batch size, and other hyperparameters. This file allows for easier replication of the training process.
- ``trainer_state.json``: Captures the state of the trainer, such as epoch information and optimizer state, which can be useful for resuming training.
- ``optimizer.pt``: Stores the optimizer's state for PyTorch models, allowing for a resumption of training from where it left off.

6. Model Card
-------------

``README.md`` or ``model_card.json``. The model card provides documentation about the model, including details about its intended use, training data, performance metrics, ethical considerations, and any limitations. This can either be a ``README.md`` or structured as a ``model_card.json``.


7. Tokenization and Feature Extraction Files
--------------------------------------------

- ``merges.txt``: For byte pair encoding (BPE) tokenizers, this file contains the merge operations used to split words into subwords.
- ``preprocessor_config.json``: Contains configuration details for any pre-processing or feature extraction steps applied to the input before passing it to the model.


Versioning
::::::::::

The `revision` field is used to determine what version of the model should be downloaded from the hub.
You can start by serving the `main` branch but once you publish your model, you should start to version it.

The `version` scheme we use is pretty loose. It can be can be `main` or a version following a extended semver:

.. code-block:: text

   [v]MAJOR.MINOR[.PATCH][.(alpha|beta|pre|post|rc|)NUMBER]

We don't provide any sorting function.

Examples:

- v1.0
- v2.3.4
- 1.2.1
- 1.0.0-beta1
- 1.0.0.alpha2
- 1.0.0.rc1

To version a model, you can push a tag on Hugging Face using `git tag v1.0 && git push --tags` and on the GCP
bucket, create a new directory where you copy the model files.
