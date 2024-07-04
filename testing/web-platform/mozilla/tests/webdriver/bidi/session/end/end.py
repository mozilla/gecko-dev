import pytest
from webdriver.bidi.error import InvalidSessionIDError

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("marionette_enabled", [False, True])
async def test_with_marionette_enabled(
    new_session, add_browser_capabilities, bidi_client, marionette_enabled
):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})},
        browser_args={"use_marionette": marionette_enabled},
    )

    await bidi_session.session.end()

    # Connect the client again.
    not_active_bidi_session = await bidi_client(
        current_browser=bidi_session.current_browser
    )

    # Make sure that command will fail, since the session was closed.
    with pytest.raises(InvalidSessionIDError):
        await not_active_bidi_session.session.end()

    # Make sure that session can be created.
    response = await not_active_bidi_session.session.status()
    assert response["ready"] is True


async def test_start_session_again(new_session, add_browser_capabilities):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )
    first_session_id = bidi_session.session_id

    await bidi_session.session.end()

    # Try to create a session again
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})}
    )

    assert isinstance(bidi_session.session_id, str)
    assert first_session_id != bidi_session.session_id


async def test_send_the_command_after_session_end(
    new_session, add_browser_capabilities, bidi_client
):
    bidi_session = await new_session(
        capabilities={"alwaysMatch": add_browser_capabilities({})},
    )

    await bidi_session.session.end()

    # Connect the client again.
    not_active_bidi_session = await bidi_client(
        current_browser=bidi_session.current_browser
    )

    # Make sure that command will fail, since the session was closed.
    with pytest.raises(InvalidSessionIDError):
        await not_active_bidi_session.session.end()
