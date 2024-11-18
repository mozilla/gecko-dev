import pytest
from support.addons import (
    get_base64_for_addon_file,
    get_ids_for_installed_addons,
    is_addon_private_browsing_allowed,
    is_addon_temporary_installed,
)
from tests.support.asserts import assert_error, assert_success

from . import install_addon, uninstall_addon


def test_install_invalid_addon(session):
    response = install_addon(session, "addon", "aGVsbG8=")
    assert_error(response, "unknown error")


@pytest.mark.parametrize("value", [True, False], ids=["required", "not required"])
def test_install_unsigned_addon_with_signature(session, use_pref, value):
    # Even though "xpinstall.signatures.required" preference is enabled in Firefox by default,
    # it's disabled for wpt tests, so test both values here.
    use_pref("xpinstall.signatures.required", value)

    response = install_addon(
        session, "addon", get_base64_for_addon_file("webextension-unsigned.xpi"), False
    )

    if value is True:
        assert_error(response, "unknown error")
    else:
        addon_id = assert_success(response)

        installed_addon_ids = get_ids_for_installed_addons(session)

        try:
            assert addon_id in installed_addon_ids
            assert addon_id == "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}"
            assert is_addon_temporary_installed(session, addon_id) is False
        finally:
            # Clean up the addon.
            uninstall_addon(session, addon_id)


def test_install_unsigned_addon_temporarily(session):
    response = install_addon(
        session, "addon", get_base64_for_addon_file("webextension-unsigned.xpi"), True
    )
    addon_id = assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    try:
        assert addon_id in installed_addon_ids
        assert addon_id == "{d3e7c1f1-2e35-4a49-89fe-9f46eb8abf0a}"
        assert is_addon_temporary_installed(session, addon_id) is True
    finally:
        # Clean up the addon.
        uninstall_addon(session, addon_id)


@pytest.mark.parametrize("temporary", [True, False])
def test_install_signed_addon(session, temporary):
    response = install_addon(
        session, "addon", get_base64_for_addon_file("amosigned.xpi"), temporary
    )
    addon_id = assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    try:
        assert addon_id in installed_addon_ids
        assert addon_id == "amosigned-xpi@tests.mozilla.org"
        assert is_addon_temporary_installed(session, addon_id) is temporary
    finally:
        # Clean up the addon.
        uninstall_addon(session, addon_id)


@pytest.mark.parametrize("allow_private_browsing", [True, False])
def test_install_addon_with_private_browsing(session, allow_private_browsing):
    response = install_addon(
        session,
        "addon",
        get_base64_for_addon_file("amosigned.xpi"),
        False,
        allow_private_browsing,
    )
    addon_id = assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    try:
        assert addon_id in installed_addon_ids
        assert addon_id == "amosigned-xpi@tests.mozilla.org"
        assert (
            is_addon_private_browsing_allowed(session, addon_id)
            is allow_private_browsing
        )
    finally:
        # Clean up the addon.
        uninstall_addon(session, addon_id)
