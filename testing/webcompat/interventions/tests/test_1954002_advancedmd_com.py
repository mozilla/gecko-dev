import pytest

URL = "https://login.advancedmd.com/"
FRAME_CSS = "#frame-login"
SUPPORTED_CSS = "#loginName"
UNSUPPORTED_TEXT = "Use Chrome, Safari or Edge"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(FRAME_CSS))
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    client.switch_to_frame(client.await_css(FRAME_CSS))
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
