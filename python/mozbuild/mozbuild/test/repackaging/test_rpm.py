# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from contextlib import nullcontext as does_not_raise

import mozunit
import pytest

from mozbuild.repackaging import rpm, utils

_APPLICATION_INI_CONTENT = """[App]
Vendor=Mozilla
Name=Firefox
RemotingName=firefox-nightly-try
CodeName=Firefox Nightly
BuildID=20230222000000
"""

_APPLICATION_INI_CONTENT_DATA = {
    "name": "Firefox",
    "display_name": "Firefox Nightly",
    "vendor": "Mozilla",
    "remoting_name": "firefox-nightly-try",
    "build_id": "20230222000000",
}


@pytest.mark.parametrize(
    "version, build_number, package_name_suffix, description_suffix, release_product, application_ini_data, expected, raises",
    (
        (
            "112.0a1",
            1,
            "",
            "",
            "firefox",
            {
                "name": "Firefox",
                "display_name": "Firefox",
                "vendor": "Mozilla",
                "remoting_name": "firefox-nightly-try",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox",
                "PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "PKG_NAME": "firefox-nightly-try",
                "PKG_VERSION": "112.0a1",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-nightly-try",
            },
            does_not_raise(),
        ),
        (
            "112.0a1",
            1,
            "-l10n-fr",
            " - Language pack for Firefox Nightly for fr",
            "firefox",
            {
                "name": "Firefox",
                "display_name": "Firefox",
                "vendor": "Mozilla",
                "remoting_name": "firefox-nightly-try",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox - Language pack for Firefox Nightly for fr",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox",
                "PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "PKG_NAME": "firefox-nightly-try-l10n-fr",
                "PKG_VERSION": "112.0a1",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-nightly-try-l10n-fr",
            },
            does_not_raise(),
        ),
        (
            "112.0b1",
            1,
            "",
            "",
            "firefox",
            {
                "name": "Firefox",
                "display_name": "Firefox",
                "vendor": "Mozilla",
                "remoting_name": "firefox-nightly-try",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox",
                "PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "PKG_NAME": "firefox-nightly-try",
                "PKG_VERSION": "112.0b1",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-nightly-try",
            },
            does_not_raise(),
        ),
        (
            "112.0",
            2,
            "",
            "",
            "firefox",
            {
                "name": "Firefox",
                "display_name": "Firefox",
                "vendor": "Mozilla",
                "remoting_name": "firefox-nightly-try",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox",
                "PKG_INSTALL_PATH": "usr/lib/firefox-nightly-try",
                "PKG_NAME": "firefox-nightly-try",
                "PKG_VERSION": "112.0",
                "PKG_BUILD_NUMBER": 2,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-nightly-try",
            },
            does_not_raise(),
        ),
        (
            "120.0b9",
            1,
            "",
            "",
            "devedition",
            {
                "name": "Firefox",
                "display_name": "Firefox Developer Edition",
                "vendor": "Mozilla",
                "remoting_name": "firefox-aurora",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox Developer Edition",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox Developer Edition",
                "PKG_INSTALL_PATH": "usr/lib/firefox-devedition",
                "PKG_NAME": "firefox-devedition",
                "PKG_VERSION": "120.0b9",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-devedition",
            },
            does_not_raise(),
        ),
        (
            "120.0b9",
            1,
            "-l10n-ach",
            " - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
            "devedition",
            {
                "name": "Firefox",
                "display_name": "Firefox Developer Edition",
                "vendor": "Mozilla",
                "remoting_name": "firefox-aurora",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox Developer Edition - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox Developer Edition",
                "PKG_INSTALL_PATH": "usr/lib/firefox-devedition",
                "PKG_NAME": "firefox-devedition-l10n-ach",
                "PKG_VERSION": "120.0b9",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-devedition-l10n-ach",
            },
            does_not_raise(),
        ),
        (
            "120.0b9",
            1,
            "-l10n-ach",
            " - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
            "devedition",
            {
                "name": "Firefox",
                "display_name": "Firefox Developer Edition",
                "vendor": "Mozilla",
                "remoting_name": "firefox-aurora",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox Developer Edition - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox Developer Edition",
                "PKG_INSTALL_PATH": "usr/lib/firefox-devedition",
                "PKG_NAME": "firefox-devedition-l10n-ach",
                "PKG_VERSION": "120.0b9",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-devedition-l10n-ach",
            },
            does_not_raise(),
        ),
        (
            "120.0b9",
            1,
            "-l10n-ach",
            " - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
            "devedition",
            {
                "name": "Firefox",
                "display_name": "Firefox Developer Edition",
                "vendor": "Mozilla",
                "remoting_name": "firefox-aurora",
                "build_id": "20230222000000",
            },
            {
                "DESCRIPTION": "Mozilla Firefox Developer Edition - Firefox Developer Edition Language Pack for Acholi (ach) – Acoli",
                "PRODUCT_NAME": "Firefox",
                "DISPLAY_NAME": "Firefox Developer Edition",
                "PKG_INSTALL_PATH": "usr/lib/firefox-aurora",
                "PKG_NAME": "firefox-aurora-l10n-ach",
                "PKG_VERSION": "120.0b9",
                "PKG_BUILD_NUMBER": 1,
                "MANPAGE_DATE": "February 22, 2023",
                "Icon": "firefox-aurora-l10n-ach",
            },
            pytest.raises(AssertionError),
        ),
    ),
)
def test_get_build_variables(
    version,
    build_number,
    package_name_suffix,
    description_suffix,
    release_product,
    application_ini_data,
    expected,
    raises,
):
    application_ini_data = utils._parse_application_ini_data(
        application_ini_data,
        version,
        build_number,
    )
    with raises:
        build_variables = rpm._get_build_variables(
            application_ini_data,
            "x86",
            version,
            release_product=release_product,
            package_name_suffix=package_name_suffix,
            description_suffix=description_suffix,
            build_number=build_number,
        )

        assert build_variables == {
            **{
                "CHANGELOG_DATE": "Wed Feb 22 2023",
                "ARCH_NAME": "x86",
                "DEPENDS": "",
            },
            **expected,
        }


@pytest.mark.parametrize(
    "does_path_exits, expectation",
    (
        (True, does_not_raise()),
        (False, pytest.raises(rpm.NoRpmPackageFound)),
    ),
)
def test_generate_rpm_archive(
    monkeypatch,
    does_path_exits,
    expectation,
):
    monkeypatch.setattr(rpm, "_get_command", lambda *_: ["mock_command"])
    monkeypatch.setattr(rpm.subprocess, "check_call", lambda *_, **__: None)

    def mock_exists(path):
        assert path == "/target_dir/x86_64/firefox-111.0-1.x86_64.rpm"
        return does_path_exits

    monkeypatch.setattr(rpm.os.path, "exists", mock_exists)

    def mock_move(source_path, destination_path):
        assert source_path == "/target_dir/x86_64/firefox-111.0-1.x86_64.rpm"
        assert destination_path == "/output/target.rpm"

    monkeypatch.setattr(rpm.shutil, "move", mock_move)

    def mock_copy(source_path, destination_path):
        assert source_path == "/tmp/firefox.tar.xz"
        assert destination_path == "/source_dir/rpm/firefox.tar.xz"

    monkeypatch.setattr(rpm.shutil, "copy", mock_copy)

    with expectation:
        rpm._generate_rpm_archive(
            source_dir="/source_dir/rpm",
            infile="/tmp/firefox.tar.xz",
            target_dir="/target_dir",
            output_file_path="/output/target.rpm",
            build_variables={
                "PKG_NAME": "firefox",
                "PKG_VERSION": "111.0",
                "PKG_BUILD_NUMBER": 1,
            },
            arch="x86_64",
        )


RPMBUILD_COMMAND = [
    "rpmbuild",
    "-ba",
    "/src/firefox.spec",
    "--define",
    "_builddir /target/build",
    "--define",
    "_rpmdir /target",
    "--define",
    "_sourcedir /src",
    "--define",
    "_srcrpmdir /target",
]


@pytest.mark.parametrize(
    "arch, is_chroot_available, expected",
    (
        (
            "all",
            True,
            [
                "chroot",
                "/srv/rpm-all",
                "bash",
                "-c",
                f"cd /tmp/*/source; {' '.join(RPMBUILD_COMMAND)}",
            ],
        ),
        ("all", False, RPMBUILD_COMMAND),
        (
            "x86",
            True,
            [
                "chroot",
                "/srv/rpm-x86",
                "bash",
                "-c",
                f"cd /tmp/*/source; {' '.join(RPMBUILD_COMMAND)}",
            ],
        ),
        ("x86", False, RPMBUILD_COMMAND),
        (
            "x86_64",
            True,
            [
                "chroot",
                "/srv/rpm-x86_64",
                "bash",
                "-c",
                f"cd /tmp/*/source; {' '.join(RPMBUILD_COMMAND)}",
            ],
        ),
        (
            "x86_64",
            False,
            RPMBUILD_COMMAND,
        ),
    ),
)
def test_get_command(monkeypatch, arch, is_chroot_available, expected):
    monkeypatch.setattr(rpm, "_is_chroot_available", lambda _: is_chroot_available)
    assert rpm._get_command("/src", "/target", arch) == expected


@pytest.mark.parametrize(
    "arch, does_dir_exist, expected_path, expected_result",
    (
        ("all", False, "/srv/rpm-all", False),
        ("all", True, "/srv/rpm-all", True),
        ("x86", False, "/srv/rpm-x86", False),
        ("x86_64", False, "/srv/rpm-x86_64", False),
        ("x86", True, "/srv/rpm-x86", True),
        ("x86_64", True, "/srv/rpm-x86_64", True),
    ),
)
def test_is_chroot_available(
    monkeypatch, arch, does_dir_exist, expected_path, expected_result
):
    def _mock_is_dir(path):
        assert path == expected_path
        return does_dir_exist

    monkeypatch.setattr(rpm.os.path, "isdir", _mock_is_dir)
    assert rpm._is_chroot_available(arch) == expected_result


@pytest.mark.parametrize(
    "arch, expected",
    (
        ("all", "/srv/rpm-all"),
        ("x86", "/srv/rpm-x86"),
        ("x86_64", "/srv/rpm-x86_64"),
    ),
)
def test_get_chroot_path(arch, expected):
    assert rpm._get_chroot_path(arch) == expected


if __name__ == "__main__":
    mozunit.main()
