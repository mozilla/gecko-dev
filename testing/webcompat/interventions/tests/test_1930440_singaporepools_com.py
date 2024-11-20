import pytest

URL = "https://online.singaporepools.com/en/lottery"

UNSUPPORTED_ALERT = "unsupported browser"


async def get_alert(client):
    alert = await client.await_alert(UNSUPPORTED_ALERT, timeout=10)
    await client.navigate(URL)
    return await alert


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await get_alert(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await get_alert(client)
