# -*- coding: utf-8 -*-"
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from typing import Dict, Set


class FailedPlatform:
    """
    Stores all failures on different build types and test variants for a single platform.
    This allows us to detect when a platform failed on all build types or all test variants to
    generate a simpler skip-if condition.
    """

    def __init__(
        self,
        # Keys are build types, values are test variants for this build type
        oop_permutations: Dict[str, list[str]],
    ) -> None:
        # Contains all test variants for each build type the task failed on
        self.failures: Dict[str, Set[str]] = {}
        self.oop_permutations = oop_permutations

    def get_possible_build_types(self) -> list[str]:
        return list(self.oop_permutations.keys())

    def get_possible_test_variants(self, build_type: str) -> list[str]:
        return self.oop_permutations.get(build_type, [])

    def is_full_fail(self) -> bool:
        """
        Test if failed on every test variant of every build type
        """
        build_types = set(self.failures.keys())
        return all(
            [
                bt in build_types and self.is_full_test_variants_fail(bt)
                for bt in self.get_possible_build_types()
            ]
        )

    def is_full_test_variants_fail(self, build_type: str) -> bool:
        """
        Test if failed on every test variant of given build type
        """
        failed_variants = self.failures.get(build_type, [])
        return all(
            [t in failed_variants for t in self.get_possible_test_variants(build_type)]
        )

    def get_cleaned_build_type(self, build_types: list[str]) -> str:
        """
        Converts the list of build types describing the task to a single build type
        to be used in the skip-if condition.
        If after cleaning there are more than 1 build type available, raise an error.
        """
        filtered_types = [
            b for b in build_types if b in self.get_possible_build_types()
        ]
        # Some build types like "asan" are also "opt"
        # We only want to add the "opt" skip condition if it is the only type present
        if "opt" in filtered_types and len(filtered_types) > 1:
            filtered_types.remove("opt")
        if len(filtered_types) == 0:
            raise ValueError(f"Could not get valid build type from {str(build_types)}")
        if len(filtered_types) > 1:
            raise ValueError(
                f"Expected a single build type after cleaning, got {str(filtered_types)}"
            )
        return filtered_types[0]

    def get_no_variant_conditions(self, and_str: str, build_type: str):
        """
        The no_variant test variant does not really exist and is only internal.
        This function gets all available test variants for the given build type
        and negates them to create a skip-if that handle tasks without test variants
        """
        variants = [
            tv
            for tv in self.get_possible_test_variants(build_type)
            if tv != "no_variant"
        ]
        return_str = ""
        for tv in variants:
            if not tv.startswith("!"):
                return_str += and_str + "!" + tv
            else:
                return_str += and_str + tv.replace("!", "", 1)
        return return_str

    def get_test_variant_condition(self, test_variant: str):
        if test_variant == "no-fission":
            return "!fission"
        return test_variant

    def get_cleaned_test_variants(self, build_type: str, test_variants: list[str]):
        converted_variants = [self.get_test_variant_condition(t) for t in test_variants]
        filtered_variants = [
            t
            for t in converted_variants
            if t in self.get_possible_test_variants(build_type)
        ]
        if len(filtered_variants) == 0:
            filtered_variants = ["no_variant"]
        return filtered_variants

    def get_skip_string(
        self, and_str: str, build_types: list[str], test_variants: list[str]
    ) -> str:
        cleaned_build_type = self.get_cleaned_build_type(build_types)
        filtered_variants = self.get_cleaned_test_variants(
            cleaned_build_type, test_variants
        )
        if self.failures.get(cleaned_build_type) is None:
            self.failures[cleaned_build_type] = set(filtered_variants)
        else:
            self.failures[cleaned_build_type].update(filtered_variants)

        return_str = ""
        # If every test variant of every build type failed, do not add anything
        if not self.is_full_fail():
            return_str += and_str + cleaned_build_type
            filtered_variants = self.get_cleaned_test_variants(
                cleaned_build_type, test_variants
            )
            if not self.is_full_test_variants_fail(cleaned_build_type):
                if len(filtered_variants) == 1 and filtered_variants[0] == "no_variant":
                    return_str += self.get_no_variant_conditions(
                        and_str, cleaned_build_type
                    )
                else:
                    for tv in filtered_variants:
                        return_str += and_str + tv

        return return_str
