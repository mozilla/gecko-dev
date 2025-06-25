import asyncio

import pytest
from webdriver.error import ElementClickInterceptedException, WebDriverException

URL = "https://salling.dk/herre/toej/c-11905/"
COOKIES_CSS = "button[class*=accept]"
COOKIES_OVERLAY_CSS = "#coiOverlay"
FILTERS_CSS = "button:has(svg.icon--filter)"
BRANDS_CSS = ".filter:nth-of-type(3):has(.accordion)"


async def are_filters_onscreen(client):
    client.set_screen_size(767, 500)
    await client.navigate(URL, wait="none")
    client.await_css(COOKIES_CSS, is_displayed=True).click()
    client.await_element_hidden(client.css(COOKIES_OVERLAY_CSS))
    for _ in range(20):
        try:
            await asyncio.sleep(0.1)
            client.await_css(FILTERS_CSS, is_displayed=True).click()
        except (WebDriverException, ElementClickInterceptedException):
            continue
        if client.find_css(BRANDS_CSS, is_displayed=True):
            break
    for _ in range(20):
        try:
            client.await_css(BRANDS_CSS, is_displayed=True).click()
            break
        except (WebDriverException, ElementClickInterceptedException):
            await asyncio.sleep(0.1)
    await asyncio.sleep(0.5)
    return client.execute_script(
        "return arguments[0].getBoundingClientRect().top >= 0",
        client.await_css(BRANDS_CSS, is_displayed=True),
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await are_filters_onscreen(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await are_filters_onscreen(client)
