import pytest

URL = "https://app.sunroom.so/"

SUPPORTED_CSS = "button[data-testid='action-button']"
UNSUPPORTED_TEXT = "browser not supported"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS)
