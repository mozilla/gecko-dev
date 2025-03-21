import pytest
from support.addons import get_ids_for_installed_addons
from tests.support.asserts import assert_error, assert_success
from tests.support.helpers import get_base64_for_extension_file

from . import install_addon, uninstall_addon


def test_uninstall_nonexistent_addon(session):
    response = uninstall_addon(session, "i-do-not-exist-as-an-id")
    assert_error(response, "unknown error")


@pytest.mark.parametrize(
    "filename, temporary",
    [("firefox/signed.xpi", True), ("firefox/unsigned.xpi", False)],
)
def test_uninstall_addon(session, filename, temporary):
    response = install_addon(
        session, "addon", get_base64_for_extension_file(filename), temporary
    )
    addon_id = assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    assert addon_id in installed_addon_ids

    response = uninstall_addon(session, addon_id)
    assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    assert addon_id not in installed_addon_ids
