# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import io
import os
import re

import six

RE_COMMENT = re.compile(r"\s+#")
RE_HTTP = re.compile(r"HTTP\((\.\.(\/\.\.)*)\)")
RE_PROTOCOL = re.compile(r"^\w+:")
FAILURE_TYPES = (
    "fails",
    "fails-if",
    "needs-focus",
    "random",
    "random-if",
    "silentfail",
    "silentfail-if",
    "skip",
    "skip-if",
    "slow",
    "slow-if",
    "fuzzy",
    "fuzzy-if",
    "require-or",
    "asserts",
    "asserts-if",
)
PREF_ITEMS = (
    "pref",
    "test-pref",
    "ref-pref",
)
RE_ANNOTATION = re.compile(r"(.*)\((.*)\)")
# NOTE: CONDITIONS_JS_TO_MP should cover the known conditions as found by
# https://searchfox.org/mozilla-central/search?q=skip-if.*+include&path=&case=false&regexp=true
# AND must be kept in sync with the parsers
# https://searchfox.org/mozilla-central/source/layout/tools/reftest/manifest.sys.mjs#47
CONDITIONS_JS_TO_MP = {  # Manifestparser expression grammar
    "Android": "(os == 'android')",
    "geckoview": "(os == 'android')",
    "useDrawSnapshot": "snapshot",
    "winWidget": "(os == 'win')",
    "win11_2009": "(os == 'win')",
    "cocoaWidget": "(os == 'mac')",
    "appleSilicon": "(os == 'mac' && processor == 'aarch64')",
    "gtkWidget": "(os == 'linux')",
    "isDebugBuild": "debug",
    "ThreadSanitizer": "tsan",
    "AddressSanitizer": "asan",
    "is64Bit": "(bits == 64)",
    "wayland": "(display == 'wayland')",
    "isCoverageBuild": "ccov",
    "&&": " && ",
    "||": " || ",
}


class ReftestManifest(object):
    """Represents a parsed reftest manifest."""

    def __init__(self, finder=None):
        self.path = None
        self.dirs = set()
        self.files = set()
        self.manifests = set()
        self.tests = []
        self.finder = finder

    def translate_condition_for_mozinfo(self, annotation):
        m = RE_ANNOTATION.match(annotation)
        if not m and annotation != "skip":
            return annotation, ""

        if annotation == "skip":
            key = "skip-if"
            condition = "true"
        else:
            key = m.group(1)
            condition = m.group(2)
            for js in CONDITIONS_JS_TO_MP:
                mp = CONDITIONS_JS_TO_MP[js]
                condition = condition.replace(js, mp)

        return key, condition

    def get_skip_if_for_mozinfo(self, parent_skip_if, annotations):
        skip_if = parent_skip_if
        for annotation in annotations:
            key, condition = self.translate_condition_for_mozinfo(annotation)
            if key == "skip-if" and condition:
                skip_if = "\n".join([t for t in [skip_if, condition] if t])
        return skip_if

    def load(self, path, parent_skip_if=""):
        """Parse a reftest manifest file."""

        def add_test(file, annotations, referenced_test=None, skip_if=""):
            if RE_PROTOCOL.match(file):
                return
            test = os.path.normpath(os.path.join(mdir, urlprefix + file))
            if test in self.files:
                # if test path has already been added, make no changes, to
                # avoid duplicate paths in self.tests
                return
            self.files.add(test)
            self.dirs.add(os.path.dirname(test))
            test_dict = {
                "path": test,
                "here": os.path.dirname(test),
                "manifest": normalized_path,
                "name": os.path.basename(test),
                "head": "",
                "support-files": "",
                "subsuite": "",
            }
            if referenced_test:
                test_dict["referenced-test"] = referenced_test

            if skip_if:
                # when we pass in a skip_if but there isn't one inside the manifest
                # (i.e. no annotations), it is important to add the inherited skip-if
                test_dict["skip-if"] = skip_if

            for annotation in annotations:
                key, condition = self.translate_condition_for_mozinfo(annotation)
                test_dict[key] = "\n".join(
                    [t for t in [test_dict.get(key, ""), condition] if t]
                )

            self.tests.append(test_dict)

        normalized_path = os.path.normpath(os.path.abspath(path))
        self.manifests.add(normalized_path)
        if not self.path:
            self.path = normalized_path

        mdir = os.path.dirname(normalized_path)
        self.dirs.add(mdir)

        if self.finder:
            lines = self.finder.get(path).read().splitlines()
        else:
            with io.open(path, "r", encoding="utf-8") as fh:
                lines = fh.read().splitlines()

        urlprefix = ""
        defaults = []
        for i, line in enumerate(lines):
            lineno = i + 1
            line = six.ensure_text(line)

            # Entire line is a comment.
            if line.startswith("#"):
                continue

            # Comments can begin mid line. Strip them.
            m = RE_COMMENT.search(line)
            if m:
                line = line[: m.start()]
            line = line.strip()
            if not line:
                continue

            items = line.split()
            if items[0] == "defaults":
                defaults = items[1:]
                continue

            items = defaults + items
            annotations = []
            for j in range(len(items)):
                item = items[j]

                if item.startswith(FAILURE_TYPES) or item.startswith(PREF_ITEMS):
                    annotations += [item]
                    continue
                if item == "HTTP":
                    continue

                m = RE_HTTP.match(item)
                if m:
                    # Need to package the referenced directory.
                    self.dirs.add(os.path.normpath(os.path.join(mdir, m.group(1))))
                    continue

                if j < len(defaults):
                    raise ValueError(
                        "Error parsing manifest {}, line {}: "
                        "Invalid defaults token '{}'".format(path, lineno, item)
                    )

                if item == "url-prefix":
                    urlprefix = items[j + 1]
                    break

                if item == "include":
                    skip_if = self.get_skip_if_for_mozinfo(parent_skip_if, annotations)
                    self.load(os.path.join(mdir, items[j + 1]), skip_if)
                    break

                if item == "load" or item == "script":
                    add_test(items[j + 1], annotations, None, parent_skip_if)
                    break

                if item == "==" or item == "!=" or item == "print":
                    add_test(items[j + 1], annotations, None, parent_skip_if)
                    add_test(items[j + 2], annotations, items[j + 1], parent_skip_if)
                    break
