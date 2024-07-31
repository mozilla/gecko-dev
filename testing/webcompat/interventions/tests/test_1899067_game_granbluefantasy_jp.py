import pytest

URL = "http://game.granbluefantasy.jp/"
SUPPORTED_CSS = "#mobage-game-container"
UNSUPPORTED_CSS = "body#browser"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert not client.find_css(UNSUPPORTED_CSS)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS)
    assert not client.find_css(SUPPORTED_CSS)
