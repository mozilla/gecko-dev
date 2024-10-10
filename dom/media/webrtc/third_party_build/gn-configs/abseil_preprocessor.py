# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.


def main(gn_config_file):
    target_dir = "abseil-cpp"
    raw_file_contents = ""
    with open(gn_config_file, "r") as fh:
        raw_file_contents = fh.read()
    raw_file_contents = raw_file_contents.replace(f"{target_dir}/", "")
    raw_file_contents = raw_file_contents.replace(f"{target_dir}:", ":")
    with open(gn_config_file, "w") as fh:
        fh.write(raw_file_contents)
