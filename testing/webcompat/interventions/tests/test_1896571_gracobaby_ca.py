import time

import pytest

URL = "https://www.gracobaby.ca/en_CA/All%20In%20Ones/SAP_2184499.html"


async def check_can_scroll(client):
    # The site only has the problem if the window size is narrow enough.
    client.set_screen_size(400, 800)
    await client.navigate(URL)

    # Check if the window scrolls by checking window.scrollY.
    old_pos = client.execute_script("return window.scrollY")
    client.apz_scroll(client.await_css("body"), dy=100)

    # We need to wait for window.scrollY to be updated, but we
    # can't rely on an event being fired to detect when it's done.
    time.sleep(1)
    return old_pos != client.execute_script("return window.scrollY")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, in_headless_mode):
    assert await check_can_scroll(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, in_headless_mode):
    assert not await check_can_scroll(client)
