import pytest
from tests.support.asserts import assert_success


def new_window(session, type_hint=None):
    return session.transport.send(
        "POST",
        "session/{session_id}/window/new".format(**vars(session)),
        {"type": type_hint},
    )


@pytest.mark.parametrize("type_hint", ["tab", "window", None])
def test_payload(session, type_hint):
    original_handles = session.handles

    response = new_window(session, type_hint=type_hint)
    value = assert_success(response)
    handles = session.handles
    assert len(handles) == len(original_handles) + 1
    assert value["handle"] in handles
    assert value["handle"] not in original_handles

    # On Android applications, have a single window only and a new tab will
    # be opened instead.
    is_android = session.capabilities["platformName"] == "android"
    if is_android or type_hint is None:
        assert value["type"] == "tab"
    else:
        assert value["type"] == type_hint
