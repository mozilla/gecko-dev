import asyncio

import pytest

URL = "https://indices.circana.com"
OLD_URL = "https://indices.iriworldwide.com"
BAD_CSS = ".hiddPage"


async def does_unsupported_banner_appear(client, url):
    await client.navigate(url, wait="complete")
    await asyncio.sleep(0.5)
    return client.find_css(BAD_CSS)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await does_unsupported_banner_appear(client, URL)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled_old_url(client):
    assert not await does_unsupported_banner_appear(client, OLD_URL)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await does_unsupported_banner_appear(client, URL)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled_old_url(client):
    assert await does_unsupported_banner_appear(client, OLD_URL)
