import pytest

URL = "https://watch.foxtel.com.au/app/#/live/fs1"
SUPPORTED_CSS = ".login-page"
UNSUPPORTED_CSS = ".ua-barrier"


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
    assert not client.find_css(SUPPORTED_CSS)
