# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from functools import reduce
from typing import Optional


class FailedPlatform:
    """
    Stores all failures on different build types and test variants for a single platform.
    This allows us to detect when a platform failed on all build types or all test variants to
    generate a simpler skip-if condition.
    """

    def __init__(
        self,
        # Keys are build types, values are test variants for this build type
        # Tests variants can be composite by using the "+" character
        # eg: a11y_checks+swgl
        # each build_type[test_variant] has a {'pass': x, 'fail': y}
        # x and y represent number of times this was run in the last 30 days
        # See examples in
        # https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/gecko.v2.mozilla-central.latest.source.test-info-all/artifacts/public%2Ftest-info-testrun-matrix.json
        oop_permutations: Optional[
            dict[
                str,  # Build type
                dict[str, dict[str, int]],  # Test Variant  # {'pass': x, 'fail': y}
            ]
        ],
        high_freq: bool = False,
    ) -> None:
        # Contains all test variants for each build type the task failed on
        self.failures: dict[str, dict[str, int]] = {}
        self.oop_permutations = oop_permutations
        self.high_freq = high_freq

    def get_possible_build_types(self) -> list[str]:
        return (
            list(self.oop_permutations.keys())
            if self.oop_permutations is not None
            else []
        )

    def get_possible_test_variants(self, build_type: str) -> list[str]:
        permutations = (
            self.oop_permutations.get(build_type, {})
            if self.oop_permutations is not None
            else []
        )
        return [tv for tv in permutations]

    def is_full_fail(self) -> bool:
        """
        Test if failed on every test variant of every build type
        """
        build_types = set(self.failures.keys())
        possible_build_types = self.get_possible_build_types()
        # If we do not have information on possible build types, do not consider it a full fail
        # This avoids creating a too broad skip-if condition
        if len(possible_build_types) == 0:
            return False
        return all(
            [
                bt in build_types and self.is_full_test_variants_fail(bt)
                for bt in possible_build_types
            ]
        )

    def is_full_high_freq_fail(self) -> bool:
        """
        Test if there are at least 7 failures on each build type
        """
        build_types = set(self.failures.keys())
        possible_build_types = self.get_possible_build_types()
        # If we do not have information on possible build types, do not consider it a full fail
        # This avoids creating a too broad skip-if condition
        if len(possible_build_types) == 0:
            return False
        return all(
            [
                bt in build_types and sum(list(self.failures[bt].values())) >= 7
                for bt in possible_build_types
            ]
        )

    def is_full_test_variants_fail(self, build_type: str) -> bool:
        """
        Test if failed on every test variant of given build type
        """
        failed_variants = self.failures.get(build_type, {}).keys()
        possible_test_variants = self.get_possible_test_variants(build_type)
        # If we do not have information on possible test variants, do not consider it a full fail
        # This avoids creating a too broad skip-if condition
        if len(possible_test_variants) == 0:
            return False
        return all([t in failed_variants for t in possible_test_variants])

    def get_negated_variant(self, test_variant: str):
        if not test_variant.startswith("!"):
            return "!" + test_variant
        return test_variant.replace("!", "", 1)

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
            return_str += and_str + self.get_negated_variant(tv)
        return return_str

    def get_test_variant_condition(
        self, and_str: str, build_type: str, test_variant: str
    ):
        """
        If the given test variant is part of another composite test variant, then add negations matching that composite
        variant to prevent overlapping in skips.
        eg: test variant "a11y_checks" is to be added while "a11y_checks+swgl" exists
        the resulting condition will be "a11y_checks && !swgl"
        """
        all_test_variants_parts = [
            tv.split("+")
            for tv in self.get_possible_test_variants(build_type)
            if tv not in ["no_variant", test_variant]
        ]
        test_variant_parts = test_variant.split("+")
        # List of composite test variants more specific than the current one
        matching_variants_parts = [
            tv_parts
            for tv_parts in all_test_variants_parts
            if all(x in tv_parts for x in test_variant_parts)
        ]
        variants_to_negate = [
            part
            for tv_parts in matching_variants_parts
            for part in tv_parts
            if part not in test_variant_parts
        ]

        return_str = reduce((lambda x, y: x + and_str + y), test_variant_parts, "")
        return_str = reduce(
            (lambda x, y: x + and_str + self.get_negated_variant(y)),
            variants_to_negate,
            return_str,
        )
        return return_str

    def get_full_test_variant_condition(
        self, and_str: str, build_type: str, test_variant: str
    ) -> str:
        if test_variant == "no_variant":
            return self.get_no_variant_conditions(and_str, build_type)
        else:
            return self.get_test_variant_condition(and_str, build_type, test_variant)

    def get_test_variant_string(self, test_variant: str):
        """
        Some test variants strings need to be updated to match what is given in oop_permutations
        """
        if test_variant == "no-fission":
            return "!fission"
        if test_variant == "1proc":
            return "!e10s"
        return test_variant

    def get_skip_string(
        self, and_str: str, build_type: str, test_variant: str
    ) -> Optional[str]:
        if self.failures.get(build_type) is None:
            self.failures[build_type] = {test_variant: 1}
        elif self.failures[build_type].get(test_variant) is None:
            self.failures[build_type][test_variant] = 1
        else:
            self.failures[build_type][test_variant] += 1

        if not self.high_freq:
            return self._get_skip_string(and_str, build_type, test_variant)
        return self._get_high_freq_skip_string(and_str, build_type)

    def _get_high_freq_skip_string(
        self, and_str: str, build_type: str
    ) -> Optional[str]:
        return_str: Optional[str] = None

        if self.is_full_high_freq_fail():
            return_str = ""
        else:
            total_failures = sum(list(self.failures[build_type].values()))
            most_variant, most_failures = self.get_test_variant_with_most_failures(
                build_type
            )

            if total_failures >= 7:
                return_str = and_str + build_type
                if most_failures / total_failures >= 3 / 4:
                    return_str += self.get_full_test_variant_condition(
                        and_str, build_type, most_variant
                    )
                elif self.is_full_fail():
                    return_str = ""

        return return_str

    def get_test_variant_with_most_failures(self, build_type: str) -> tuple[str, int]:
        most_failures = 0
        most_variant = ""
        for variant, failures in self.failures[build_type].items():
            if failures > most_failures:
                most_failures = failures
                most_variant = variant
        return most_variant, most_failures

    def _get_skip_string(
        self, and_str: str, build_type: str, test_variant: str
    ) -> Optional[str]:
        return_str = ""
        # If every test variant of every build type failed, do not add anything
        if not self.is_full_fail():
            return_str += and_str + build_type
            if not self.is_full_test_variants_fail(build_type):
                return_str += self.get_full_test_variant_condition(
                    and_str, build_type, test_variant
                )

        return return_str
