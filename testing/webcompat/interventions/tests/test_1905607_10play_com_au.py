import pytest

URL = "https://10play.com.au/masterchef/episodes/season-16"
SUPPORTED_TEXT = "Sign in to watch this video"
UNSUPPORTED_TEXT = "Your mobile browser is not supported"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(SUPPORTED_TEXT)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
    assert not client.find_text(SUPPORTED_TEXT)
