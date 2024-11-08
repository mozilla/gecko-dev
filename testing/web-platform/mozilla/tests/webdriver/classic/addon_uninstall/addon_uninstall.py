import pytest
from support.addons import (
    get_base64_for_addon_file,
    get_ids_for_installed_addons,
)
from tests.support.asserts import assert_error, assert_success


def install_addon(session, addon, temp=False):
    return session.transport.send(
        "POST",
        f"/session/{session.session_id}/moz/addon/install",
        {"addon": addon, "temporary": temp},
    )


def uninstall_addon(session, addon_id):
    return session.transport.send(
        "POST",
        f"/session/{session.session_id}/moz/addon/uninstall",
        {"id": addon_id},
    )


def test_uninstall_nonexistent_addon(session):
    response = uninstall_addon(session, "i-do-not-exist-as-an-id")
    assert_error(response, "unknown error")


@pytest.mark.parametrize(
    "filename, temporary",
    [("amosigned.xpi", True), ("webextension-unsigned.xpi", False)],
)
def test_uninstall_addon(session, filename, temporary):
    response = install_addon(session, get_base64_for_addon_file(filename), temporary)
    addon_id = assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    assert addon_id in installed_addon_ids

    response = uninstall_addon(session, addon_id)
    assert_success(response)

    installed_addon_ids = get_ids_for_installed_addons(session)

    assert addon_id not in installed_addon_ids
