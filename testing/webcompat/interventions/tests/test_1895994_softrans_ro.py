import asyncio

import pytest
from webdriver import NoSuchElementException

URL = "https://www.softrans.ro/tarife.html"

COOKIES_CSS = "button[classname=analyticsPermited]"


async def can_scroll_horizontally(client):
    await client.navigate(URL)

    try:
        client.await_css(COOKIES_CSS, is_displayed=True, timeout=3).click()
    except NoSuchElementException:
        pass

    body = client.await_css("body")

    # First confirm that the site's contents are still too wide for the viewport.
    assert client.execute_script(
        """
      const s = document.scrollingElement;
      return s.clientWidth < s.scrollWidth;
    """
    )

    expected_pos = client.execute_script("return window.scrollY")
    client.apz_scroll(body, dx=1000)
    await asyncio.sleep(0.2)
    final_pos = client.execute_script("return window.scrollX")
    return expected_pos != final_pos


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_scroll_horizontally(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_scroll_horizontally(client)
