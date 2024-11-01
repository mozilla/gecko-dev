import pytest

URL = "https://mychildrensla-demo.avizia.com/"

SUPPORTED_CSS = "login-page-component"
UNSUPPORTED_CSS = "img[src*='chrome']"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS)
