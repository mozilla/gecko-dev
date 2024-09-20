Models management
=================

When Firefox uses a model, it will

1. read metadata stored in Remote Settings
2. download model files from our hub
3. store the files in IndexDB


1. Remote Settings
::::::::::::::::::

We have two collections in Remote Settings:

- `ml-onnx-runtime`: provides all the WASM files we need to run the inference platform.
- `ml-inference-options`: provides for each `taskId` a list of running options, such as the `modelId`.

Running the inference API will download the WASM files if needed, and then see
if there's an entry for the task in `ml-inference-options`, to grab the options.

That allows us to set the default running options for each task.

This is also how we can update a model without changing Firefox's code:
setting a new revision for a model in Remote Settings will trigger a new download for our users.


2. Model Hub
::::::::::::

Our Model hub follows the same structure than Hugging Face, each file for a model is under
a unique URL:

  `https://model-hub.mozilla.org/<organization>/<model>/<revision>/<path>`

Where:
- `organization` and `name` are the model id. example " `mozilla/distivit`"
- `revision` is the branch or version
- `path` is the path to the file.

When a model is stored in the Mozilla or Hugging Face Model Hub, it typically consists of several
files that define the model, its configuration, tokenizer, and training metadata.

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


3. IndexDB
::::::::::

Model files are stored in IndexDB so users don't need to download them again.
