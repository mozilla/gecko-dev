import pytest

URL = "https://www.bmo.com/main/personal/credit-cards/getting-started"

SUPPORTED_TEXT = "Before you apply"
UNSUPPORTED_TEXT = "use another web browser"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_text(SUPPORTED_TEXT)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
