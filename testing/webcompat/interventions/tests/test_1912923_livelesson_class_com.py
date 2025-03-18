import pytest

URL = "https://livelesson.class.com/class/8a731bd4-17f2-4bf6-a1b4-4f3561cab2bd"

SUPPORTED_TEXT = "Join your class session"
UNSUPPORTED_TEXT = "use Microsoft Edge or Google Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_text(SUPPORTED_TEXT, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_text(SUPPORTED_TEXT, is_displayed=True)
