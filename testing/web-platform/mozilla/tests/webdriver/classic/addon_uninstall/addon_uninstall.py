from tests.support.asserts import assert_error

from . import uninstall_addon


def test_uninstall_nonexistent_addon(session):
    response = uninstall_addon(session, "i-do-not-exist-as-an-id")
    assert_error(response, "unknown error")
