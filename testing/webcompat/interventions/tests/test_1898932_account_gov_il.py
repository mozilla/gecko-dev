import asyncio

import pytest

URL = "https://account.gov.il/sspr/public/newuser"

SUPPORTED_CSS = "#givenName"
UNSUPPORTED_CSS = "#bgBrowserCheck"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    await asyncio.sleep(1)
    assert not client.is_displayed(client.find_css(UNSUPPORTED_CSS))


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    await asyncio.sleep(1)
    client.await_css(UNSUPPORTED_CSS, is_displayed=True)
