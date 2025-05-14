import pytest

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("type_hint", ["tab", "window"])
async def test_type_hint(bidi_session, current_session, type_hint):
    assert len(await bidi_session.browsing_context.get_tree()) == 1
    assert len(await bidi_session.browser.get_client_windows()) == 1

    await bidi_session.browsing_context.create(type_hint=type_hint)

    if type_hint == "window":
        expected_window_count = 2
    else:
        expected_window_count = 1

    assert len(await bidi_session.browsing_context.get_tree()) == 2
    assert len(await bidi_session.browser.get_client_windows()) == expected_window_count
