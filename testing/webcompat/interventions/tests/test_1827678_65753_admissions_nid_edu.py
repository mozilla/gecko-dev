import pytest

URL = "https://admissions.nid.edu/"

SUPPORTED_CSS = "nav"
UNSUPPORTED_TEXT = "please login using only Google Chrome"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.disable_window_alert()
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
