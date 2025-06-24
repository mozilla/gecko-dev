import pytest

URL = "https://nvidia.com/en-eu"


async def is_content_offscreen(client):
    await client.navigate(URL)
    return client.execute_script("return document.body.scrollWidth > window.innerWidth")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_content_offscreen(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_content_offscreen(client)
