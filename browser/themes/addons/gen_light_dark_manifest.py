# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import copy
import json

OVERRIDES = {
    "light": {
        "name": "Light",
        "description": "A theme with a light color scheme.",
        "browser_specific_settings": {
            "gecko": {"id": "firefox-compact-light@mozilla.org"}
        },
    },
    "dark": {
        "name": "Dark",
        "description": "A theme with a dark color scheme.",
        "browser_specific_settings": {
            "gecko": {"id": "firefox-compact-dark@mozilla.org"}
        },
    },
}


def gen_light_dark_manifest(output_manifest, input_manifest, variant):
    assert variant == "light" or variant == "dark"
    theme_key = "theme" if variant == "light" else "dark_theme"

    input = json.loads(open(input_manifest).read())
    output = copy.deepcopy(input)

    del output["dark_theme"]
    output["theme"] = input[theme_key]
    output |= OVERRIDES[variant]

    output_manifest.write(json.dumps(output, indent=2).encode("utf-8"))


def gen_light_manifest(output_manifest, input_manifest):
    return gen_light_dark_manifest(output_manifest, input_manifest, "light")


def gen_dark_manifest(output_manifest, input_manifest):
    return gen_light_dark_manifest(output_manifest, input_manifest, "dark")
