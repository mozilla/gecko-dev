import asyncio

import pytest
from webdriver.error import StaleElementReferenceException

URL = "https://www.exploretock.com/city/san-antonio/search?city=San%20Antonio&date=2024-10-22&latlng=29.4241219%2C-98.4936282&size=2&time=18%3A00&type=DINE_IN_EXPERIENCES"
MOBILE_MAP_BUTTON_CSS = "#maincontent .AvailabilitySearchResults button"
GOOD_MSG = "Attempted to load a Vector Map, but failed. Falling back to Raster."
BAD_MSG = "blocked a worker script (worker-src) at blob:https://www.exploretock.com/"


async def check_get_correct_console_msg(client, platform, msg):
    await client.navigate(URL, wait="none")
    promise = await client.promise_console_message_listener(msg)
    if platform == "android":
        for _ in range(5):
            try:
                client.await_css(
                    MOBILE_MAP_BUTTON_CSS,
                    condition="elem.innerText.includes('Map')",
                    is_displayed=True,
                ).click()
                break
            except StaleElementReferenceException:
                asyncio.sleep(1)
    assert await promise


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, platform):
    await check_get_correct_console_msg(client, platform, GOOD_MSG)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, platform):
    await check_get_correct_console_msg(client, platform, BAD_MSG)
