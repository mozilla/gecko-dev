import pytest
from support.addons import get_ids_for_installed_addons
from tests.support.asserts import assert_error, assert_success
from tests.support.helpers import get_base64_for_extension_file

from . import install_addon, uninstall_addon


def test_uninstall_nonexistent_addon(session):
    response = uninstall_addon(session, "i-do-not-exist-as-an-id")
    assert_error(response, "unknown error")
