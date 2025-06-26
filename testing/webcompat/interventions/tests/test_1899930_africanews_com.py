import pytest

URL = "https://africanews.com/"

POPUP_CSS = "#didomi-notice-agree-button"
UNSUPPORTED_TEXT = "upgrade your browser"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    client.await_css(POPUP_CSS, is_displayed=True).click()
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    client.await_css(POPUP_CSS, is_displayed=True).click()
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
