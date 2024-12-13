# -*- coding: utf-8 -*-"
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import pytest
from failedplatform import FailedPlatform
from mozunit import main


def test_get_possible_build_types():
    """Test get_possible_build_types"""

    fp = FailedPlatform({})
    assert fp.get_possible_build_types() == []

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_possible_build_types() == ["build_type1"]

    fp = FailedPlatform(
        {"build_type1": ["test_variant1"], "build_type2": ["test_variant1"]}
    )
    assert fp.get_possible_build_types() == ["build_type1", "build_type2"]


def test_get_possible_test_variants():
    """Test get_possible_test_variants"""

    fp = FailedPlatform({})
    assert fp.get_possible_test_variants("") == []

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_possible_test_variants("unknown") == []

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_possible_test_variants("build_type1") == ["test_variant1"]

    fp = FailedPlatform(
        {"build_type1": ["test_variant1"], "build_type2": ["test_variant2"]}
    )
    assert fp.get_possible_test_variants("build_type2") == ["test_variant2"]

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    assert fp.get_possible_test_variants("build_type1") == [
        "test_variant1",
        "test_variant2",
    ]


def test_is_full_test_variants_fail():
    """Test is_full_test_variants_fail"""

    fp = FailedPlatform({})
    assert fp.is_full_test_variants_fail("")

    fp = FailedPlatform({"build_type1": []})
    assert fp.is_full_test_variants_fail("build_type1")

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert not fp.is_full_test_variants_fail("build_type1")

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    fp.failures["build_type1"] = {"test_variant2"}
    assert not fp.is_full_test_variants_fail("build_type1")

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    fp.failures["build_type1"] = {"test_variant1"}
    assert fp.is_full_test_variants_fail("build_type1")

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    fp.failures["build_type1"] = {"test_variant1"}
    assert not fp.is_full_test_variants_fail("build_type1")

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    fp.failures["build_type1"] = {"test_variant1", "test_variant2"}
    assert fp.is_full_test_variants_fail("build_type1")


def test_is_full_fail():
    """Test is_full_fail"""

    fp = FailedPlatform({})
    assert fp.is_full_fail()

    fp = FailedPlatform({"build_type1": []})
    assert not fp.is_full_fail()

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert not fp.is_full_fail()

    fp = FailedPlatform({"build_type1": []})
    fp.failures["build_type1"] = set()
    assert fp.is_full_fail()

    fp = FailedPlatform({"build_type1": []})
    fp.failures["build_type2"] = set()
    assert not fp.is_full_fail()

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    fp.failures["build_type1"] = set()
    assert not fp.is_full_fail()

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    fp.failures["build_type1"] = {"test_variant1"}
    assert fp.is_full_fail()

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    fp.failures["build_type1"] = {"test_variant1"}
    assert not fp.is_full_fail()

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    fp.failures["build_type1"] = {"test_variant1", "test_variant2"}
    assert fp.is_full_fail()

    fp = FailedPlatform(
        {
            "build_type1": ["test_variant1", "test_variant2"],
            "build_type2": ["test_variant1", "test_variant2"],
        }
    )
    fp.failures["build_type1"] = {"test_variant1", "test_variant2"}
    assert not fp.is_full_fail()

    fp = FailedPlatform(
        {
            "build_type1": ["test_variant1", "test_variant2"],
            "build_type2": ["test_variant1", "test_variant2"],
        }
    )
    fp.failures["build_type1"] = {"test_variant1", "test_variant2"}
    fp.failures["build_type2"] = {"test_variant1", "test_variant2"}
    assert fp.is_full_fail()


def test_get_cleaned_build_types():
    """Test get_cleaned_build_types"""

    # Raise an error if no valid build types found
    fp = FailedPlatform({})
    with pytest.raises(ValueError):
        fp.get_cleaned_build_type([])
    with pytest.raises(ValueError):
        fp.get_cleaned_build_type(["build_type1"])
    fp = FailedPlatform({"build_type1": []})
    with pytest.raises(ValueError):
        fp.get_cleaned_build_type([])
    with pytest.raises(ValueError):
        fp.get_cleaned_build_type(["build_type2"])

    # Only keep declared build types
    fp = FailedPlatform({"build_type1": []})
    assert fp.get_cleaned_build_type(["build_type1"]) == "build_type1"
    assert fp.get_cleaned_build_type(["build_type1", "build_type2"]) == "build_type1"

    # Removes opt if not the only build type
    fp = FailedPlatform({"build_type1": [], "opt": []})
    assert fp.get_cleaned_build_type(["opt", "build_type1"]) == "build_type1"
    assert fp.get_cleaned_build_type(["opt"]) == "opt"

    # Fail if more than one build type remaining after clean
    fp = FailedPlatform({"build_type1": [], "build_type2": []})
    with pytest.raises(ValueError):
        fp.get_cleaned_build_type(["build_type1", "build_type2"])


def test_get_no_variant_conditions():
    """Test get_no_variant_conditions"""

    fp = FailedPlatform({})
    assert fp.get_no_variant_conditions(" && ", "build_type1") == ""

    fp = FailedPlatform({"build_type1": []})
    assert fp.get_no_variant_conditions(" && ", "build_type1") == ""

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_no_variant_conditions(" && ", "build_type1") == " && !test_variant1"

    # Handle already negated test variants
    fp = FailedPlatform({"build_type1": ["test_variant1", "!fission"]})
    assert (
        fp.get_no_variant_conditions(" && ", "build_type1")
        == " && !test_variant1 && fission"
    )


def test_get_test_variant_condition():
    """Test get_no_variant_conditions"""

    # Simply returns the given variant if no composite variant exists
    fp = FailedPlatform({})
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1"
    )

    fp = FailedPlatform({"build_type1": []})
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1"
    )

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1"
    )

    # Returns the negation of the composite if any
    fp = FailedPlatform(
        {"build_type1": ["test_variant1", "test_variant1+test_variant2"]}
    )
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1 && !test_variant2"
    )

    # Ignores composite variants it is not part of
    fp = FailedPlatform(
        {"build_type1": ["test_variant1", "test_variant2+test_variant3"]}
    )
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1"
    )

    # Convert a composite test variant
    fp = FailedPlatform(
        {"build_type1": ["test_variant1", "test_variant1+test_variant2"]}
    )
    assert (
        fp.get_test_variant_condition(
            " && ", "build_type1", "test_variant1+test_variant2"
        )
        == " && test_variant1 && test_variant2"
    )

    # Handles composite variants included in other composites
    fp = FailedPlatform(
        {
            "build_type1": [
                "test_variant1+test_variant2",
                "test_variant1+test_variant2+test_variant3",
            ]
        }
    )
    assert (
        fp.get_test_variant_condition(
            " && ", "build_type1", "test_variant1+test_variant2"
        )
        == " && test_variant1 && test_variant2 && !test_variant3"
    )

    # Detects if the composite or in scrambled order
    fp = FailedPlatform(
        {
            "build_type1": [
                "test_variant2+test_variant1",
                "test_variant1+test_variant3+test_variant2",
            ]
        }
    )
    assert (
        fp.get_test_variant_condition(
            " && ", "build_type1", "test_variant2+test_variant1"
        )
        == " && test_variant2 && test_variant1 && !test_variant3"
    )

    # Handles when variant is included in several composites
    fp = FailedPlatform(
        {
            "build_type1": [
                "test_variant1+test_variant2",
                "test_variant1+test_variant2+test_variant3",
                "test_variant1+test_variant4+test_variant2",
            ]
        }
    )
    assert (
        fp.get_test_variant_condition(
            " && ", "build_type1", "test_variant2+test_variant1"
        )
        == " && test_variant2 && test_variant1 && !test_variant3 && !test_variant4"
    )

    # Simply returns the current composite variant if it is not fully contained in another
    fp = FailedPlatform(
        {"build_type1": ["test_variant1+test_variant2", "test_variant1+test_variant3"]}
    )
    assert (
        fp.get_test_variant_condition(
            " && ", "build_type1", "test_variant1+test_variant2"
        )
        == " && test_variant1 && test_variant2"
    )

    # Ignore matching composite variants in other build types
    fp = FailedPlatform(
        {
            "build_type1": ["test_variant1"],
            "build_type2": ["test_variant1+test_variant2"],
        }
    )
    assert (
        fp.get_test_variant_condition(" && ", "build_type1", "test_variant1")
        == " && test_variant1"
    )


def test_get_cleaned_test_variants():
    """Test get_cleaned_test_variants"""

    fp = FailedPlatform({})
    assert fp.get_cleaned_test_variant("build_type1", []) == "no_variant"
    assert fp.get_cleaned_test_variant("build_type1", ["test_variant1"]) == "no_variant"

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_cleaned_test_variant("build_type1", []) == "no_variant"
    assert (
        fp.get_cleaned_test_variant("build_type1", ["test_variant1"]) == "test_variant1"
    )
    assert fp.get_cleaned_test_variant("build_type1", ["test_variant2"]) == "no_variant"

    fp = FailedPlatform({"build_type1": ["!fission"]})
    assert fp.get_cleaned_test_variant("build_type1", ["no-fission"]) == "!fission"

    fp = FailedPlatform({"build_type1": ["test_variant1+test_variant2"]})
    assert (
        fp.get_cleaned_test_variant("build_type1", ["test_variant1", "test_variant2"])
        == "test_variant1+test_variant2"
    )


def test_get_skip_string():
    """Test get_skip_string"""

    # Full fails on single line => nothing returned
    fp = FailedPlatform({"build_type1": []})
    assert fp.get_skip_string(" && ", ["build_type1"], []) == ""

    fp = FailedPlatform({"build_type1": ["test_variant1"]})
    assert fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"]) == ""

    fp = FailedPlatform({"build_type1": ["test_variant1+test_variant2"]})
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1", "test_variant2"])
        == ""
    )

    # Fail only on some build types without test variant
    # => only failed build types returned
    fp = FailedPlatform({"build_type1": [], "build_type2": []})
    assert fp.get_skip_string(" && ", ["build_type1"], []) == " && build_type1"

    # Fail only on one build type with test variant
    # => only failed build types returned with negated test variants
    fp = FailedPlatform({"build_type1": ["test_variant1"], "build_type2": []})
    assert (
        fp.get_skip_string(" && ", ["build_type1"], [])
        == " && build_type1 && !test_variant1"
    )

    # Full test variant fail on single line => only build type returned
    fp = FailedPlatform({"build_type1": ["test_variant1"], "build_type2": []})
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"])
        == " && build_type1"
    )

    fp = FailedPlatform(
        {"build_type1": ["test_variant1+test_variant2"], "build_type2": []}
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1", "test_variant2"])
        == " && build_type1"
    )

    # Fail only on some test variants => build type and test variant returned
    fp = FailedPlatform(
        {"build_type1": ["test_variant1", "test_variant2"], "build_type2": []}
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"])
        == " && build_type1 && test_variant1"
    )

    # Full fail on second call
    fp = FailedPlatform({"build_type1": [], "build_type2": []})
    assert fp.get_skip_string(" && ", ["build_type1"], []) == " && build_type1"
    assert fp.get_skip_string(" && ", ["build_type2"], []) == ""

    # Full fail on second call with test variants
    fp = FailedPlatform(
        {
            "build_type1": ["test_variant1+test_variant2"],
            "build_type2": ["test_variant1+test_variant2"],
        }
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1", "test_variant2"])
        == " && build_type1"
    )
    assert (
        fp.get_skip_string(" && ", ["build_type2"], ["test_variant1", "test_variant2"])
        == ""
    )

    fp = FailedPlatform({"build_type1": ["test_variant1", "test_variant2"]})
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"])
        == " && build_type1 && test_variant1"
    )
    assert fp.get_skip_string(" && ", ["build_type1"], ["test_variant2"]) == ""

    # Fail on variant and no_variant
    fp = FailedPlatform(
        {"build_type1": ["test_variant1", "no_variant"], "build_type2": []}
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"])
        == " && build_type1 && test_variant1"
    )
    assert fp.get_skip_string(" && ", ["build_type1"], []) == " && build_type1"

    # Complex cases
    fp = FailedPlatform(
        {
            "build_type1": [
                "test_variant1",
                "test_variant2",
                "test_variant1+test_variant2",
            ],
            "build_type2": ["no_variant", "test_variant1", "test_variant2"],
        }
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1"])
        == " && build_type1 && test_variant1 && !test_variant2"
    )
    assert (
        fp.get_skip_string(" && ", ["build_type2"], [])
        == " && build_type2 && !test_variant1 && !test_variant2"
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant2"])
        == " && build_type1 && test_variant2 && !test_variant1"
    )
    assert (
        fp.get_skip_string(" && ", ["build_type1"], ["test_variant1", "test_variant2"])
        == " && build_type1"
    )
    assert (
        fp.get_skip_string(" && ", ["build_type2"], ["test_variant1"])
        == " && build_type2 && test_variant1"
    )
    assert fp.get_skip_string(" && ", ["build_type2"], ["test_variant2"]) == ""


if __name__ == "__main__":
    main()
