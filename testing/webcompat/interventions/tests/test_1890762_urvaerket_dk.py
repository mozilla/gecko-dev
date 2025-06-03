import asyncio

import pytest

URL = "https://www.urvaerket.dk/herreure-c-5097.html"
COOKIES_CSS = "button[class*=accept]"
COOKIES_OVERLAY_CSS = "#coiOverlay"
FILTER_CSS = ".filter_products .fp"
FILTER_CONTENT_CSS = ".filter-content"
MAERKE_CSS = ".filter-content .fpbx"


async def can_scroll_filters(client):
    await client.navigate(URL, wait="none")
    client.await_css(COOKIES_CSS, is_displayed=True).click()
    client.await_element_hidden(client.css(COOKIES_OVERLAY_CSS))
    client.await_css(FILTER_CSS, is_displayed=True).click()
    client.await_css(MAERKE_CSS, is_displayed=True).click()
    content = client.await_css(FILTER_CONTENT_CSS, is_displayed=True)
    await asyncio.sleep(1)
    top = client.execute_script("return arguments[0].scrollTop", content)
    for i in range(20):
        await asyncio.sleep(0.1)
        client.apz_scroll(content, dy=100)
        if top != client.execute_script("return arguments[0].scrollTop", content):
            return True
    return False


@pytest.mark.only_platforms("android")
@pytest.mark.actual_platform_required
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_scroll_filters(client)


@pytest.mark.only_platforms("android")
@pytest.mark.actual_platform_required
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_scroll_filters(client)
