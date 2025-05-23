# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def get_signed_artifacts(input, formats, behavior=None):
    """
    Get the list of signed artifacts for the given input and formats.
    """
    artifacts = set()
    if input.endswith(".dmg"):
        artifacts.add(input.replace(".dmg", ".tar.gz"))
        if behavior and behavior != "mac_sign":
            artifacts.add(input.replace(".dmg", ".pkg"))
    else:
        artifacts.add(input)

    if "gcp_prod_autograph_gpg" in formats:
        artifacts.add(f"{input}.asc")

    return artifacts
