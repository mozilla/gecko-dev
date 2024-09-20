Notifications
=============

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
