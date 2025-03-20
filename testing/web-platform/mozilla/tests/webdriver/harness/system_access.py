import pytest
from support.context import using_context
from support.helpers import clear_pref
from webdriver import error


def test_enabled_by_default(session):
    with pytest.raises(error.UnsupportedOperationException):
        with using_context(session, "chrome"):
            session.window_handle


@pytest.mark.capabilities(
    {
        "moz:firefoxOptions": {
            "prefs": {
                "remote.system-access-check.enabled": False,
            },
        },
    }
)
def test_opt_out_with_preference(session):
    try:
        with using_context(session, "chrome"):
            session.window_handle
    finally:
        # Clear preference so that it doesn't leak into following tests
        clear_pref(session, "remote.system-access-check.enabled")
