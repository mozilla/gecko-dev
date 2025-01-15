# -*- coding: utf-8 -*-"
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from copy import deepcopy

from mozinfo.platforminfo import PlatformInfo
from mozunit import main

BASE_TEST_SETTINGS = {
    "platform": {
        "os": {
            "name": "linux",
            "version": "2204",
            "build": None,
        },
        "arch": "x86",
    },
    "build": {"type": "debug"},
    "runtime": {},
}


def test_os():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # Android an linux names do not change
    test_settings["platform"]["os"]["name"] = "linux"
    test_settings["platform"]["os"]["version"] = "22.04"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os == "linux"

    test_settings["platform"]["os"]["name"] = "android"
    test_settings["platform"]["os"]["version"] = "13.0"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os == "android"

    # Macosx and Windows names are shortened
    test_settings["platform"]["os"]["name"] = "macosx"
    test_settings["platform"]["os"]["version"] = "1407"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os == "mac"

    test_settings["platform"]["os"]["name"] = "windows"
    test_settings["platform"]["os"]["version"] = "11"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os == "win"


def test_os_version():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # linux and macosx version get expanded
    test_settings["platform"]["os"]["name"] = "linux"
    test_settings["platform"]["os"]["version"] = "2204"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os_version == "22.04"

    test_settings["platform"]["os"]["name"] = "macosx"
    test_settings["platform"]["os"]["version"] = "1470"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os_version == "14.70"

    # Android os version gets converted to sdk version
    test_settings["platform"]["os"]["name"] = "android"
    test_settings["platform"]["os"]["version"] = "14.0"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os_version == "34"

    # Windows version stays as is
    test_settings["platform"]["os"]["name"] = "windows"
    test_settings["platform"]["os"]["version"] = "11"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os_version == "11"

    # Can add build number if needed for windows
    test_settings["platform"]["os"]["name"] = "windows"
    test_settings["platform"]["os"]["version"] = "11"
    test_settings["platform"]["os"]["build"] = "2009"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.os_version == "11.2009"


def test_os_arch():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # detects 32bits arch as x86
    test_settings["platform"]["arch"] = "x86"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "x86"
    test_settings["platform"]["arch"] = "anything32"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "x86"

    # detects specific architectures
    test_settings["platform"]["arch"] = "aarch64"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "aarch64"
    test_settings["platform"]["arch"] = "ppc"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "ppc"
    test_settings["platform"]["arch"] = "arm7"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "arm7"

    # converts other arch as x86_64
    test_settings["platform"]["arch"] = "x86_64"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "x86_64"
    test_settings["platform"]["arch"] = "anything"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.arch == "x86_64"


def test_os_bits():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # detects 32bits
    test_settings["platform"]["arch"] = "x86"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.bits == "32"
    test_settings["platform"]["arch"] = "anything32"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.bits == "32"

    # other architectures are assumed 64 bits
    test_settings["platform"]["arch"] = "aarch64"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.bits == "64"
    test_settings["platform"]["arch"] = "x86_64"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.bits == "64"
    test_settings["platform"]["arch"] = "anything"
    platform_info = PlatformInfo(test_settings)
    assert platform_info.bits == "64"


def test_build_type():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # detects opt and debug build types
    test_settings["build"] = {"type": "debug"}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "debug"
    assert platform_info.debug
    test_settings["build"] = {"type": "opt"}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt

    # detects more complex build types
    test_settings["build"] = {"type": "opt", "asan": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "asan"
    assert platform_info.opt

    # ignore shippable, devedition and mingwclang
    test_settings["build"] = {"type": "opt", "shippable": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt

    test_settings["build"] = {"type": "opt", "devedition": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt

    test_settings["build"] = {"type": "opt", "mingwclang": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt

    # ignore ccov only on mac
    test_settings["platform"]["os"]["name"] = "macosx"
    test_settings["platform"]["os"]["version"] = "1407"
    test_settings["build"] = {"type": "opt", "ccov": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt
    test_settings["platform"]["os"]["name"] = "linux"
    test_settings["platform"]["os"]["version"] = "2204"
    test_settings["build"] = {"type": "opt", "ccov": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "ccov"
    assert platform_info.opt

    # ignore lite on android
    test_settings["platform"]["os"]["name"] = "android"
    test_settings["platform"]["os"]["version"] = "13.0"
    test_settings["build"] = {"type": "opt", "lite": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.build_type == "opt"
    assert platform_info.opt


def test_runtimes():
    test_settings = deepcopy(BASE_TEST_SETTINGS)

    # replace empty array by no_variant
    test_settings["runtime"] = {}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "no_variant"

    # ignore invalid runtimes
    test_settings["runtime"] = {"anything": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "no_variant"

    # detect valid runtimes
    test_settings["runtime"] = {"xorigin": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "xorigin"

    # converts variants using mowinfo
    test_settings["runtime"] = {"1proc": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "e10s"

    # specific logic for no-fission
    test_settings["runtime"] = {"no-fission": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "!fission"

    # combines multiple runtimes
    test_settings["runtime"] = {"xorigin": True, "1proc": True}
    platform_info = PlatformInfo(test_settings)
    assert platform_info.test_variant == "xorigin+e10s"


if __name__ == "__main__":
    main()
