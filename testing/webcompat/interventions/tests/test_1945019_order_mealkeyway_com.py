import asyncio

import pytest

URL = "https://order.mealkeyway.com/merchant/74755a54504e625163684d713635544d544c414b77413d3d/main"
LANDING_PAGE_CSS = ".landingPage"
WARNING_TEXT = "please open in Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(LANDING_PAGE_CSS, is_displayed=True)
    await asyncio.sleep(1)
    assert not client.find_text(WARNING_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_text(WARNING_TEXT, is_displayed=True)
