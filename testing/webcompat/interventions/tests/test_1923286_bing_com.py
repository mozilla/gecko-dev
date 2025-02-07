import asyncio
from asyncio.exceptions import TimeoutError

import pytest
from webdriver import NoSuchElementException

URL = "https://www.bing.com/images/feed?form=Z9LH"

IMAGE_SEARCH_CSS = "#sb_sbi"
EXAMPLE_IMAGE_CSS = "img.sbiDmImg"
PAGES_WITH_THIS_TEXT = "Pages with this"
EXTERNAL_LINK_CSS = ".tab-content.pim a.richImgLnk img"


async def does_link_work(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(IMAGE_SEARCH_CSS, is_displayed=True))
    await asyncio.sleep(1)
    client.soft_click(client.await_css(EXAMPLE_IMAGE_CSS, is_displayed=True))
    await asyncio.sleep(2)
    try:
        client.soft_click(
            client.await_text(PAGES_WITH_THIS_TEXT, is_displayed=True, timeout=5)
        )
    except NoSuchElementException:
        # Sometimes the page loads incorrectly and doesn't show "pages with this image",
        # and it isn't easy to just reload/click around to fix it.
        pytest.xfail("Page loaded incorrectly; please try again")
        return False
    await asyncio.sleep(1)
    link = client.await_css(EXTERNAL_LINK_CSS, is_displayed=True)

    url = client.execute_script(
        """
      return arguments[0].closest("a.richImgLnk").href;
    """,
        link,
    )

    nav = await client.promise_navigation_begins(url=url, timeout=2)
    client.mouse.click(link).perform()
    try:
        await nav
        return True
    except TimeoutError:
        return False


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await does_link_work(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await does_link_work(client)
