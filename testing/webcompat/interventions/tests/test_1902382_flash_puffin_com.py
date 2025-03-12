import pytest

URL = "https://flash.puffin.com/store/"

UNSUPPORTED_TEXT = "Please use latest Google Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
