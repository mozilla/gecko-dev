import pytest
from webdriver import error

from support.context import using_context


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
    with using_context(session, "chrome"):
        session.window_handle
