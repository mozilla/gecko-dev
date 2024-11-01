import pytest

URL = "https://www.yourtexasbenefits.com/Learn/Home"

SUPPORTED_CSS = "nav"
UNSUPPORTED_TEXT = "What internet browser do I need to use"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
