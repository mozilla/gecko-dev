# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file should only contain code that will be acceptable in the context of
# a moz.build sandbox.


# This function turns a metrics.yaml path (eg. "dom/push/metrics.yaml") into a
# header file name (eg. "DomPushMetrics").
# Used by both moz.build and build_scripts/glean_parser_ext/run_glean_parser.py
def convert_yaml_path_to_header_name(filepath):
    # The general rule is to remove the yaml extension then add an
    # upper case letter for each folder separator, - or _ character .
    filepath = filepath[: -(len(".yaml"))]
    path_components = filepath.replace("-", "_").split("/")

    # There are some exceptions to make some names shorter nicer:
    #  NameBaseMetrics -> NameMetrics
    #  NameBaseSomethingMetrics -> NameSomethingMetrics
    if path_components[1] == "base":
        path_components.pop(1)
    # NameComponentsSomethingMetrics -> NameSomethingMetrics
    # BrowserComponentsSomethingMetrics -> SomethingMetrics
    # ToolkitComponentsSomethingMetrics -> SomethingMetrics
    if len(path_components) > 3 and path_components[1] == "components":
        path_components.pop(1)
        if path_components[0] in ["browser", "toolkit"]:
            path_components.pop(0)

    # ModulesSomethingMetrics -> SomethingMetrics
    if len(path_components) > 2 and path_components[0] == "modules":
        path_components.pop(0)

    # MobileSharedModulesGeckoviewMetrics -> GeckoviewMetrics
    if filepath.startswith("mobile/shared/modules/") and len(path_components) > 4:
        path_components = ["geckoview", "metrics"]

    path_components = "_".join(path_components).split("_")
    return "".join(
        [
            path_component[0].upper() + path_component[1:]
            for path_component in path_components
        ]
    )
