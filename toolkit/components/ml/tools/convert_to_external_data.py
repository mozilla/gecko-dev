# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import json
import os
import pathlib
import shutil
import tempfile
from typing import Annotated

import onnx
import typer

# To use:
# pip install onnx typer
# python convert_to_external_data --base-path "/path/to/directory"


def convert(model_path, save_path):
    model = onnx.load(model_path)

    external_data_name = f"{pathlib.Path(model_path).stem}.onnx_data"

    # Create the new model in a temporary directory and copy all it's content back to save_path
    # Doing this because if save_path is same as model_path & we directly write to model_path
    # onnx will append to the external data path which would make it grow more than expected.
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_model_path = os.path.join(tmp_dir, os.path.basename(model_path))

        onnx.save_model(
            model,
            tmp_model_path,
            save_as_external_data=True,
            location=external_data_name,
        )

        file_names = os.listdir(tmp_dir)
        target_dir = str(pathlib.Path(save_path).parent)
        os.makedirs(target_dir, exist_ok=True)
        for file_name in file_names:
            shutil.copy2(os.path.join(tmp_dir, file_name), target_dir)


def main(base_path: Annotated[str, typer.Option()]):
    """
    This will convert recursively all onnx models in that directory to one with external data format.
    """
    # Convert all
    for model_path in glob.glob(
        os.path.join(base_path, "**/*.onnx"),
        recursive=True,
    ):
        print("Converting", model_path)
        convert(model_path, model_path)

    # Find all config.json and enable use_external_data_format
    for config_path in glob.glob(
        os.path.join(base_path, "**/config.json"),
        recursive=True,
    ):
        print("Modifying", config_path)
        # Load the JSON file
        with open(config_path, "r") as infile:
            config_data = json.load(infile)

        config_data["transformers.js_config"] = config_data.get(
            "transformers.js_config", {}
        )
        config_data["transformers.js_config"]["use_external_data_format"] = True

        # Save the JSON file with additional config
        with open(config_path, "w") as outfile:
            json.dump(config_data, outfile, indent=4, ensure_ascii=False)


if __name__ == "__main__":
    typer.run(main)
