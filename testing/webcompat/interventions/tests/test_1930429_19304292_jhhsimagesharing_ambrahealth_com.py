import asyncio

import pytest

URL = "https://jhhsimagesharing.ambrahealth.com/share/jh_outside_upload"

UNSUPPORTED_CSS = "#not-supported"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(UNSUPPORTED_CSS)
    await asyncio.sleep(2)
    client.await_css(UNSUPPORTED_CSS, is_displayed=False)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_css(UNSUPPORTED_CSS, is_displayed=True)
