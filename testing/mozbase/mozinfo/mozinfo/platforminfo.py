# -*- coding: utf-8 -*-"
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from typing import Any, Dict, Optional

import yaml


class PlatformInfo:

    variant_data = {}

    # From https://developer.android.com/tools/releases/platforms
    android_os_to_sdk_map = {
        "7.0": "24",
        "7.1": "25",
        "8.0": "26",
        "8.1": "27",
        "9.0": "28",
        "10.0": "29",
        "11.0": "30",
        "12.0": "31",
        "12L": "32",
        "13.0": "33",
        "14.0": "34",
    }

    buildmap = {
        "debug-isolated-process": "isolated-process",
    }

    def __init__(self, test_settings: Optional[Dict[str, Any]] = None) -> None:
        if test_settings is None:
            return

        self._platform: Dict[str, Any] = test_settings["platform"]
        self._platform_os: Dict[str, str] = self._platform["os"]
        self._build: Dict[str, str] = test_settings["build"]
        self._runtime: Dict[str, str] = test_settings.get("runtime", {})

        self.build = self._platform_os.get("build")
        self.display = self._platform.get("display")
        self.os = self._clean_os()
        self.os_version = self._clean_os_version()
        self.arch = self._clean_arch()
        self.bits = self._get_bits()
        self.build_type = self._clean_build_type()
        self.opt = self._build["type"] == "opt"
        self.debug = self._build["type"] == "debug"
        self.test_variant = self._clean_test_variant()

    def _clean_os(self) -> str:
        name = self._platform_os["name"]
        if name is None:
            raise Exception("Could not find platform name")

        pretty = name
        if pretty == "windows":
            pretty = "win"
        elif pretty == "macosx":
            pretty = "mac"

        supported_os = ("win", "mac", "linux", "android")
        if pretty not in supported_os:
            raise ValueError(
                f"Unknown os name {pretty}. Supported os are {supported_os}"
            )

        return pretty

    def _clean_os_version(self) -> str:
        cleaned_name = self.os
        version = self._platform_os["version"]
        if version is None:
            raise Exception("Could not find platform version")

        if cleaned_name in ["mac", "linux"]:
            return version[0:2] + "." + version[2:4]
        if cleaned_name == "android":
            android_version = self.android_os_to_sdk_map.get(version)
            if android_version is None:
                raise Exception(
                    f"Unknown android OS version {version}. Supported versions are {list(self.android_os_to_sdk_map.keys())}."
                )
            return android_version

        build = self.build
        if build is not None and cleaned_name == "win":
            version += "." + build
        return version

    def _clean_arch(self) -> str:
        arch = self._platform["arch"]
        if arch is None:
            raise Exception("Could not find platform architecture")

        if arch == "x86" or arch.find("32") >= 0:
            return "x86"
        elif arch not in ("aarch64", "ppc", "arm7"):
            return "x86_64"
        return arch

    def _get_bits(self) -> str:
        cleaned_arch = self.arch
        if cleaned_arch == "x86":
            return "32"
        return "64"

    def _clean_build_type(self):
        build_type = self._build["type"]
        keys = self._build.keys()
        if len(keys) > 1:
            filtered_types = [x for x in keys if x not in ["type", "shippable"]]
            if len(filtered_types) > 0:
                build_type = filtered_types[0]

        build_type = self.buildmap.get(build_type, build_type)

        # TODO: this is a hack, but these don't apply:
        if build_type in ["devedition", "mingwclang"]:  # only on beta, no mozinfo
            build_type = "opt"
        if self.os == "mac" and build_type == "ccov":  # not scheduled
            build_type = "opt"
        if (
            self.os == "android" and build_type == "lite"
        ):  # no specific way to skip this, treat as normal android
            build_type = "opt"
        return build_type

    def get_variant_data(self):
        if self.variant_data:
            return self.variant_data

        # if running locally via `./mach ...`, assuming running from root of repo
        filename = (
            os.environ.get("GECKO_PATH", ".") + "/taskcluster/kinds/test/variants.yml"
        )
        with open(filename, "r") as f:
            self.variant_data = yaml.safe_load(f.read())

        return self.variant_data

    def get_variant_condition(self, test_variant: str) -> str:
        variant_data = self.get_variant_data()
        if test_variant not in variant_data.keys():
            return ""

        mozinfo = variant_data[test_variant].get("mozinfo", "")

        # This is a hack as we have no-fission and fission variants
        # sharing a common mozinfo variable.
        # TODO: what other hacks like this exist?
        if test_variant in ["no-fission"]:
            mozinfo = "!" + mozinfo
        return mozinfo

    def _clean_test_variant(self) -> str:
        # TODO: consider adding display here
        runtimes = list(self._runtime.keys())
        test_variant = "+".join(
            [v for v in [self.get_variant_condition(x) for x in runtimes] if v]
        )
        if not runtimes or not test_variant:
            test_variant = "no_variant"
        return test_variant

    # Used for test data
    def from_dict(self, data: Dict[str, Any]):
        self.os = data["os"]
        self.os_version = data["os_version"]
        self.arch = data["arch"]
        self.bits = data["bits"]
        self.build = data.get("build")
        self.display = data.get("display")
        self.build_type = data["build_type"]
        self.opt = data["opt"]
        self.debug = data["debug"]
        self.test_variant = data["runtime"]

    def to_dict(self):
        return {
            "arch": self.arch,
            "bits": self.bits,
            "build": self.build,
            "build_type": self.build_type,
            "debug": self.debug,
            "display": self.display,
            "opt": self.opt,
            "os": self.os,
            "os_version": self.os_version,
            "runtime": self.test_variant,
        }
