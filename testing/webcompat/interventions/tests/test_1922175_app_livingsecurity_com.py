import pytest

URL = "https://app.livingsecurity.com/"

SUPPORTED_CSS = "main.login-id"
UNSUPPORTED_CSS = ".message-for-ie-users-wrapper"


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
