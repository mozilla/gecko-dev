#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os.path

import yaml

HERE = os.path.abspath(os.path.dirname(__file__))
FILTER_SETS_FILE = os.path.join(HERE, "gtest_filter_sets.yml")


def get(filter_set_name):
    """
    Retrieve the gtest filter associated with a specific filter set name.
    """

    with open(FILTER_SETS_FILE, "r") as file:
        filter_set_data = yaml.safe_load(file)

    if filter_set_name in filter_set_data:
        return filter_set_data[filter_set_name].get("gtest_filter", None)

    return None


def list():
    """
    Lists all available filter sets and their filters from the YAML file.
    """
    with open(FILTER_SETS_FILE, "r") as file:
        filter_set_data = yaml.safe_load(file)

    print("Filter sets from {}:".format(FILTER_SETS_FILE))
    for key, value in filter_set_data.items():
        gtest_filter = value.get("gtest_filter", "No gtest filter defined")
        print(f"- {key}: {gtest_filter}")
