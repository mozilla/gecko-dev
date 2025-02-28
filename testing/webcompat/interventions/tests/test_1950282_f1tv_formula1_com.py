import pytest

URL = "https://f1tv.formula1.com/"

SUPPORTED_TEXT = "Watch now"
UNSUPPORTED_TEXT = "Unsupported Browser"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(SUPPORTED_TEXT, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_text(SUPPORTED_TEXT, is_displayed=True)
