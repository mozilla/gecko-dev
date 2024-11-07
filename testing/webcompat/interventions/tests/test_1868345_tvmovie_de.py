import asyncio

import pytest
from webdriver import NoSuchElementException

URL = "https://www.tvmovie.de/tv/fernsehprogramm"

POPUP_CSS = "#sp_message_container_1179875"


async def check_if_scroll_bounces(client):
    await client.navigate(URL)

    try:
        client.await_css(POPUP_CSS, is_displayed=True, timeout=3).click()
    except NoSuchElementException:
        pass

    # If we scroll down a few times, the page will start scrolling on
    # its own when things are broken. As such we can read window.scrollY
    # to see if it changes on its own after a moment, without our help.
    client.apz_scroll(client.await_css("body"), dy=100000)
    await asyncio.sleep(0.2)
    client.apz_scroll(client.await_css("body"), dy=100000)
    await asyncio.sleep(0.2)
    client.apz_scroll(client.await_css("body"), dy=100000)
    await asyncio.sleep(0.2)
    client.apz_scroll(client.await_css("body"), dy=100000)
    await asyncio.sleep(0.2)
    expected_pos = client.execute_script("return window.scrollY")
    await asyncio.sleep(0.2)
    final_pos = client.execute_script("return window.scrollY")
    return expected_pos != final_pos


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, in_headless_mode):
    assert not await check_if_scroll_bounces(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, in_headless_mode):
    assert await check_if_scroll_bounces(client)
