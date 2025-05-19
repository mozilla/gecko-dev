# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import urllib.error
import urllib.request

from mozbuild.vendor.moz_yaml import load_moz_yaml
from mozlint import result
from mozlint.pathutils import expand_exclusions


class UpdatebotValidator:
    def __init__(self):
        self._bmo_cache = {}

    def lint_file(self, path, **kwargs):
        if not kwargs.get("testing", False):
            if not path.endswith("moz.yaml"):
                return None
            if "test/files/updatebot" in path:
                return None

        try:
            yaml = load_moz_yaml(path)

            if "vendoring" in yaml and yaml["vendoring"].get("flavor", None) == "rust":
                yaml_revision = yaml["origin"]["revision"]

                with open("Cargo.lock") as f:
                    for line in f:
                        if yaml_revision in line:
                            break
                    else:
                        return f"Revision {yaml_revision} specified in {path} wasn't found in Cargo.lock"

            if "bugzilla" in yaml:
                product = yaml["bugzilla"].get("product")
                component = yaml["bugzilla"].get("component")
                if product and component:
                    if not self._is_valid_bmo_category(product, component):
                        return (
                            f"Invalid Bugzilla product/component in {path}: '{product} / {component}'.\n"
                            f"If {path} was recently modified, it's likely the Bugzilla information was not correctly supplied.\n"
                            f"If it has not been recently modified, it's likely the Bugzilla component was recently renamed and it must be updated."
                        )

            return None
        except Exception as e:
            return f"Could not load {path} according to schema in moz_yaml.py: {e}"

    def _is_valid_bmo_category(self, product, component):
        cache_key = (product, component)
        if cache_key in self._bmo_cache:
            return self._bmo_cache[cache_key]

        url = (
            f"https://bugzilla.mozilla.org/rest/component?"
            f"product={urllib.parse.quote(product)}&component={urllib.parse.quote(component)}"
        )

        try:
            with urllib.request.urlopen(url, timeout=5) as response:
                if response.status == 200:
                    data = json.load(response)
                    valid = "error" not in data
                    self._bmo_cache[cache_key] = valid
                    return valid
        except urllib.error.HTTPError as e:
            # Most likely a 404-style result with a useful JSON error body
            try:
                data = json.load(e)
                if data.get("error") == 1:
                    self._bmo_cache[cache_key] = False
                    return False
            except Exception:
                return True  # Be lenient
        except Exception:
            return True  # Be lenient

        return True  # Be lenient


def lint(paths, config, **lintargs):
    if not isinstance(paths, list):
        paths = [paths]

    errors = []
    files = list(expand_exclusions(paths, config, lintargs["root"]))

    validator = UpdatebotValidator()
    for f in files:
        message = validator.lint_file(f, **lintargs)
        if message:
            errors.append(result.from_config(config, path=f, message=message))

    return errors
