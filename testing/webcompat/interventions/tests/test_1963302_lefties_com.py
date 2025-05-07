import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.lefties.com/mx/mujer/zapatos/tenis-c1030272270.html"
COOKIES_CSS = "#onetrust-consent-sdk"


async def does_scrolling_jitter(client):
    await client.navigate(URL)

    try:
        client.remove_element(client.await_css(COOKIES_CSS, timeout=4))
    except NoSuchElementException:
        pass

    body = client.await_css("body")
    old_pos = client.execute_script("return window.scrollY")
    for i in range(100):
        client.apz_scroll(body, dy=100)
        new_pos = client.execute_script("return window.scrollY")
        if new_pos < old_pos:
            return True
        old_pos = new_pos
    return False


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await does_scrolling_jitter(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await does_scrolling_jitter(client)
