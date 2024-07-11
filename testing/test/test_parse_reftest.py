# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest
from mozunit import main
from parse_reftest import ListManifestParser


@pytest.fixture
def lmp():
    return ListManifestParser(implicit_vars=True, verbose=True)


@pytest.mark.parametrize(
    "condition, skip_if, expected",
    [
        ("gtkWidget", "gtkWidget", "gtkWidget"),
        (
            "gtkWidget&&!is64Bit",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "gtkWidget",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "winWidget&&!is64Bit",
            "gtkWidget",
            "gtkWidget",
        ),
        ("gtkWidget&&optimized", "gtkWidget&&isDebugBuild", "gtkWidget"),
        (
            "winWidget&&!is64Bit&&optimized",
            "winWidget&&!is64Bit&&isDebugBuild",
            "winWidget&&!is64Bit",
        ),
        (
            "winWidget&&!is64Bit",
            "winWidget&&AddressSanitizer&&!fission",
            # 3 dimensions change "winWidget&&(is64Bit||!is64Bit)&&(fission||!fission)",
            "winWidget&&AddressSanitizer&&!fission",
        ),
        (
            "winWidget&&isCoverageBuild",
            "cocoaWidget&&isCoverageBuild",
            "cocoaWidget&&isCoverageBuild",
        ),
        (
            "cocoaWidget&&ThreadSanitizer&&swgl",
            "cocoaWidget",
            "cocoaWidget&&(swgl||!swgl)",
        ),
        (
            "(gtkWidget||winWidget)&&(is64Bit||!is64Bit)",
            "cocoaWidget&&!is64Bit",
            "cocoaWidget&&!is64Bit",
        ),
        (
            "winWidget&&(is64Bit||!is64Bit)&&(fission||!fission)",
            "winWidget&&!is64Bit",
            "winWidget&&(fission||!fission)&&(is64Bit||!is64Bit)",
        ),
        (
            "Android&&!swgl",
            "Android&&optimized&&!fission",
            "Android&&optimized&&!fission",
        ),
        (
            "Android&&!swgl",
            "Android&&!fission",
            "Android&&!fission",
        ),
        (
            "Android&&!swgl",
            "Android&&optimized",
            "Android",
        ),
        (
            "(gtkWidget||winWidget)",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "useDrawSnapshot",
            "Android&&!fission",
            "Android&&!fission",
        ),
        (
            "gtkWidget&&AddressSanitizer&&!fission",
            "gtkWidget&&AddressSanitizer&&!fission&&swgl",
            "gtkWidget&&AddressSanitizer&&!fission&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&AddressSanitizer&&!fission&&(swgl||!swgl)",
            "gtkWidget&&AddressSanitizer&&swgl",
            "gtkWidget&&AddressSanitizer&&(fission||!fission)&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&AddressSanitizer&&(fission||!fission)&&(swgl||!swgl)",
            "gtkWidget&&ThreadSanitizer&&swgl",
            "gtkWidget&&(fission||!fission)&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&AddressSanitizer&&(fission||!fission)&&(swgl||!swgl)",
            "gtkWidget&&isDebugBuild&&!fission&&swgl",
            "gtkWidget&&(fission||!fission)&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&(fission||!fission)&&(swgl||!swgl)",
            "gtkWidget&&AddressSanitizer&&!fission",
            "gtkWidget&&(fission||!fission)&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&swgl",
            "gtkWidget&&isDebugBuild&&!fission&&swgl",
            "gtkWidget&&(fission||!fission)&&swgl",
        ),
        (
            "gtkWidget&&!fission&&swgl",
            "gtkWidget&&isDebugBuild&&!fission&&swgl",
            "gtkWidget&&!fission&&swgl",
        ),
        (
            "gtkWidget&&!fission&&swgl",
            "gtkWidget&&!fission&&(swgl||!swgl)",
            "gtkWidget&&!fission&&(swgl||!swgl)",
        ),
        (
            "gtkWidget&&swgl",
            "gtkWidget&&!fission&&(swgl||!swgl)",
            "gtkWidget&&(fission||!fission)&&(swgl||!swgl)",
        ),
    ],
)
def test_merge_implicit(lmp, condition, skip_if, expected):
    """Test merge (implicit_vars)"""

    if lmp.should_merge(condition, skip_if):
        optimized = lmp.merge(condition, skip_if)
    else:
        optimized = skip_if
    assert optimized == expected


@pytest.mark.parametrize(
    "condition, skip_if, expected",
    [
        ("gtkWidget", "gtkWidget&&is64Bit", "gtkWidget"),
        ("gtkWidget", "gtkWidget&&!is64Bit", "gtkWidget"),
        ("gtkWidget&&!is64Bit", "gtkWidget", "gtkWidget"),
        (
            "gtkWidget&&!is64Bit",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "gtkWidget",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "winWidget&&!is64Bit",
            "gtkWidget",
            "gtkWidget",
        ),
        ("gtkWidget&&optimized", "gtkWidget&&isDebugBuild", "gtkWidget"),
        (
            "winWidget&&!is64Bit&&optimized",
            "winWidget&&!is64Bit&&isDebugBuild",
            "winWidget&&!is64Bit",
        ),
        (
            "winWidget&&!is64Bit",
            "winWidget&&AddressSanitizer&&!fission",
            "winWidget&&AddressSanitizer&&!fission",
        ),
        (
            "winWidget&&isCoverageBuild",
            "cocoaWidget&&isCoverageBuild",
            "cocoaWidget&&isCoverageBuild",
        ),
        (
            "cocoaWidget&&ThreadSanitizer&&swgl",
            "cocoaWidget",
            "cocoaWidget",
        ),
        (
            "(gtkWidget||winWidget)&&(is64Bit||!is64Bit)",
            "cocoaWidget",
            "cocoaWidget",
        ),
        (
            "winWidget&&is64Bit",
            "winWidget&&!is64Bit",
            "winWidget",
        ),
        (
            "winWidget",
            "winWidget&&!is64Bit",
            "winWidget",
        ),
        (
            "winWidget&&!is64Bit",
            "winWidget",
            "winWidget",
        ),
        (
            "winWidget",
            "winWidget&&is64Bit",
            "winWidget",
        ),
        (
            "winWidget&&is64Bit",
            "winWidget",
            "winWidget",
        ),
        (
            "Android&&!swgl",
            "Android&&optimized&&!fission",
            "Android&&optimized&&!fission",
        ),
        (
            "Android&&!swgl",
            "Android&&!fission",
            "Android&&!fission",
        ),
        (
            "Android&&!swgl",
            "Android&&optimized",
            "Android",
        ),
        (
            "(gtkWidget||winWidget)",
            "winWidget&&!is64Bit",
            "winWidget&&!is64Bit",
        ),
        (
            "useDrawSnapshot",
            "Android&&!fission",
            "Android&&!fission",
        ),
        (
            "gtkWidget&&AddressSanitizer&&!fission",
            "gtkWidget&&AddressSanitizer&&!fission&&swgl",
            "gtkWidget&&AddressSanitizer&&!fission",
        ),
        (
            "gtkWidget&&AddressSanitizer&&!fission",
            "gtkWidget&&AddressSanitizer&&swgl",
            "gtkWidget&&AddressSanitizer&&swgl",
        ),
        (
            "gtkWidget&&AddressSanitizer",
            "gtkWidget&&ThreadSanitizer",
            "gtkWidget",
        ),
        (
            "gtkWidget&&AddressSanitizer",
            "gtkWidget&&isDebugBuild&&!fission",
            "gtkWidget&&isDebugBuild&&!fission",  # 2 dimensions
        ),
        (
            "gtkWidget",
            "gtkWidget&&AddressSanitizer&&!fission",
            "gtkWidget",
        ),
        (
            "gtkWidget&&swgl",
            "gtkWidget&&isDebugBuild&&!fission&&swgl",
            "gtkWidget&&swgl",
        ),
        (
            "gtkWidget&&!fission&&swgl",
            "gtkWidget&&isDebugBuild&&!fission&&swgl",
            "gtkWidget&&!fission&&swgl",
        ),
        (
            "gtkWidget&&!fission&&swgl",
            "gtkWidget&&!fission",
            "gtkWidget&&!fission",
        ),
        (
            "gtkWidget&&swgl",
            "gtkWidget&&!fission",
            "gtkWidget&&!fission",  # 2 dimensions
        ),
        (
            "cocoaWidget&&useDrawSnapshot",
            "cocoaWidget&&useDrawSnapshot",
            "cocoaWidget",
        ),
        (
            "gtkWidget&&!useDrawSnapshot",
            "gtkWidget&&!useDrawSnapshot",
            "gtkWidget&&!useDrawSnapshot",
        ),
        (
            "gtkWidget&&!useDrawSnapshot",
            "gtkWidget",
            "gtkWidget",
        ),
    ],
)
def test_merge_explicit(lmp, condition, skip_if, expected):
    """Test merge (explicit vars)"""

    lmp.implicit_vars = False
    if lmp.should_merge(condition, skip_if):
        merged = lmp.merge(condition, skip_if)
    else:
        merged = skip_if
    assert merged == expected


if __name__ == "__main__":
    main()
