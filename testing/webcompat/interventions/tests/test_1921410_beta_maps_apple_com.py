import pytest

URL = "https://beta.maps.apple.com/"
BLOCKED_CSS = "#unsupported"
NOT_BLOCKED_CSS = "#shell-container"


@pytest.mark.only_platforms("linux")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(NOT_BLOCKED_CSS)
    assert not client.find_css(BLOCKED_CSS)


@pytest.mark.only_platforms("linux")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(BLOCKED_CSS)
    assert not client.find_css(NOT_BLOCKED_CSS)
