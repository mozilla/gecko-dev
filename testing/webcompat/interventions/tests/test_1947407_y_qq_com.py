import pytest

URL = "https://y.qq.com/artists/"

SUPPORTED_CSS = "#app"
UNSUPPORTED_CSS = ".popup_browser .icon_chrome"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
