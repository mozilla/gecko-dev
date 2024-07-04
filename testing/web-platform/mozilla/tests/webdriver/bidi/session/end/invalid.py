import pytest
from webdriver.bidi.error import InvalidSessionIDError

pytestmark = pytest.mark.asyncio


async def test_without_session(bidi_client, browser):
    current_browser = browser(use_bidi=True)

    # Connect the client.
    bidi_session = await bidi_client(current_browser=current_browser)
    response = await bidi_session.session.status()

    assert response["ready"] is True

    with pytest.raises(InvalidSessionIDError):
        await bidi_session.session.end()
