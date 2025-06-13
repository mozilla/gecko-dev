# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import sys
from pathlib import Path
from typing import Optional

from manifestparser.manifestparser import ManifestParser
from manifestparser.toml import alphabetize_toml_str, remove_skip_if
from moztest.resolve import TestResolver
from tomlkit.toml_document import TOMLDocument

ERROR = "error"


class CleanSkipfails:

    def __init__(
        self,
        command_context=None,
        manifest_search_path: str = "",
        os_name: Optional[str] = None,
        os_version: Optional[str] = None,
        processor: Optional[str] = None,
    ) -> None:
        self.command_context = command_context
        if self.command_context is not None:
            self.topsrcdir = self.command_context.topsrcdir
        else:
            self.topsrcdir = Path(__file__).parent.parent
        self.topsrcdir = os.path.normpath(self.topsrcdir)
        self.component = "clean-skip-fails"

        self.manifest_search_path = manifest_search_path
        self.os_name = os_name
        self.os_version = os_version
        self.processor = processor

    def error(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.ERROR, self.component, {ERROR: str(e)}, "ERROR: {error}"
            )
        else:
            print(f"ERROR: {e}", file=sys.stderr, flush=True)

    def info(self, e):
        if self.command_context is not None:
            self.command_context.log(
                logging.INFO, self.component, {ERROR: str(e)}, "INFO: {error}"
            )
        else:
            print(f"INFO: {e}", file=sys.stderr, flush=True)

    def full_path(self, filename: str):
        """Returns full path for the relative filename"""

        return os.path.join(self.topsrcdir, os.path.normpath(filename.split(":")[-1]))

    def isdir(self, filename: str):
        """Returns True if filename is a directory"""

        return os.path.isdir(self.full_path(filename))

    def run(self):
        if self.os_name is None and self.os_version is None and self.processor is None:
            self.error("Needs at least --os, --os_version or --processor to be set.")
            return

        manifest_path_set = self.get_manifest_paths()
        parser = ManifestParser(use_toml=True, document=True)
        for manifest_path in manifest_path_set:
            parser.read(manifest_path)
            manifest: TOMLDocument = parser.source_documents[
                os.path.abspath(manifest_path)
            ]
            has_removed_items = remove_skip_if(
                manifest, self.os_name, self.os_version, self.processor
            )
            manifest_str = alphabetize_toml_str(manifest)
            if len(manifest_str) > 0:
                fp = open(manifest_path, "w", encoding="utf-8", newline="\n")
                fp.write(manifest_str)
                fp.close()
                removed_condition: list[str] = []
                if self.os_name is not None:
                    removed_condition.append(f"'os == {self.os_name}'")
                if self.os_version is not None:
                    removed_condition.append(f"'os_version == {self.os_version}'")
                if self.processor is not None:
                    removed_condition.append(f"'processor == {self.processor}'")
                if has_removed_items:
                    self.info(
                        f'Removed skip-if conditions for {", ".join(removed_condition)} in manifest: "{manifest_path}"'
                    )
                else:
                    self.info(
                        f'Did not find skip-if conditions to remove for {", ".join(removed_condition)} in manifest: "{manifest_path}"'
                    )

    def get_manifest_paths(self) -> set[str]:
        resolver = TestResolver.from_environment(cwd=self.manifest_search_path)
        if self.isdir(self.manifest_search_path):
            tests = list(resolver.resolve_tests(paths=[self.manifest_search_path]))
            manifest_paths = set(t["manifest"] for t in tests)
        else:
            myPath = Path(".").parent
            manifest_paths = set(
                [
                    str(x).replace("\\", "/")
                    for x in myPath.glob(self.manifest_search_path)
                ]
            )
        return manifest_paths
