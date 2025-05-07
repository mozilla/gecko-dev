"""Defines the characteristics of an iOS version number."""

import re

import attr

from mozilla_version.parser import positive_int_or_none

from .version import ShipItVersion


@attr.s(frozen=True, eq=False, hash=True)
class MobileIosVersion(ShipItVersion):
    """
    Class representing an iOS version number.

    iOS version numbers are a bit different in that they don't have a patch number
    but they have a beta one.
    """

    beta_number = attr.ib(type=int, converter=positive_int_or_none, default=None)

    _VALID_ENOUGH_VERSION_PATTERN = re.compile(
        r"""
        ^(?P<major_number>\d+)
        \.(?P<minor_number>\d+)
        (b(?P<beta_number>\d+))?$""",
        re.VERBOSE,
    )

    _OPTIONAL_NUMBERS = ("beta_number",)
    _ALL_NUMBERS = ShipItVersion._MANDATORY_NUMBERS + _OPTIONAL_NUMBERS

    def __str__(self):
        """
        Format the version as a string.

        Because iOS is different, the format is "major.minor(.beta)".
        """
        version = f"{self.major_number}.{self.minor_number}"

        if self.beta_number is not None:
            version += f"b{self.beta_number}"

        return version

    def _create_bump_kwargs(self, field):
        """
        Create a version bump for the required field.

        Version bumping is a bit different for iOS as we don't have a patch number
        despite shipit expecting one. So when asked to bump the patch version, we bump
        the minor instead.
        """
        if field == "patch_number":
            field = "minor_number"

        kwargs = super()._create_bump_kwargs(field)

        # If we get a bump request for anything but the beta number, remove it
        if field != "beta_number":
            del kwargs["beta_number"]

        return kwargs

    @property
    def is_beta(self):
        """Returns true if the version is considered a beta one."""
        return self.beta_number is not None

    @property
    def is_release_candidate(self):
        """
        Returns true if the version is a release candidate.

        For iOS versions, this is always false.
        """
        return False

    def _compare(self, other):
        """Compare this release with another."""
        if isinstance(other, str):
            other = MobileIosVersion.parse(other)
        if not isinstance(other, MobileIosVersion):
            raise ValueError(f'Cannot compare "{other}", type not supported!')

        difference = super()._compare(other)
        if difference != 0:
            return difference

        return self._substract_other_number_from_this_number(other, "beta_number")
