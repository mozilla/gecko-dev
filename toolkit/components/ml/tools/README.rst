Firefox AI Platform Tools
=========================

The `toolkit/components/ml/tools` directory contains various tools designed to support the ML Engine. Below is a detailed description of the tools available in this directory.

External Data Format Conversion Script
---------------------------------------

### Overview
The `convert_to_external_data.py` script, located in `toolkit/components/ml/tools/`, provides functionality for converting ONNX models to use the external data format. This format separates large tensor data into external binary files, improving the handling of models with significant data sizes.

### Features
- **Recursive Conversion**: The script identifies all ONNX models (files with the `.onnx` extension) within a directory, including subdirectories, and processes them.
- **Inline Updates**: The conversion is performed directly within the source directory, replacing the original models with their external data format equivalents.
- **Transformers.js Config Updates**: The script identifies model configuration files (config.json) and updates them to instruct Transformers.js to use the external ONNX data files.

### Usage
1. Place your ONNX models in the desired directory.
2. Run the script from the command line:

.. code-block:: bash

  python toolkit/components/ml/tools/convert_to_external_data.py "<directory_path>"

Replace `<directory_path>` with the path to the directory containing your models.
The script will process each `.onnx` file and convert it to the external data format.

### Notes
- Ensure that you have the necessary Python dependencies installed before running the script.
- The original models will be replaced. Back up your files if you need to retain the unconverted versions.
