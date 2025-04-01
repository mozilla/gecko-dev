import asyncio

import pytest

URL = "https://www.arboretumapartments.com/"


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    await asyncio.sleep(2)
    assert not client.find_css("#browser-warning-popup")
