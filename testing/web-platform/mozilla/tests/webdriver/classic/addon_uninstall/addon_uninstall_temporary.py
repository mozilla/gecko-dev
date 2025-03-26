import pytest
from support.addons import get_ids_for_installed_addons
from tests.support.asserts import assert_success
from tests.support.helpers import get_base64_for_extension_file

from . import install_addon, uninstall_addon

# Bug 1956510: Temporary file to disable the following test, which permanently fails in CI
# because the signed web extension was already installed


@pytest.mark.allow_system_access
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
