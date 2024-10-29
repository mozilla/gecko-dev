import time

import pytest
from webdriver.error import ElementClickInterceptedException, NoSuchElementException

URL = "https://www.afisha.ru/msk/theatre/"
POPUP_CLOSE_CSS = "[data-popmechanic-close]"
COOKIES_CSS = "button.Txfw7"
OPT_CSS = "button[data-name='price']"
SLIDER_CSS = "input[type='range'][data-index='0']"


async def slider_is_clickable(client):
    await client.navigate(URL)

    popups = [client.css(COOKIES_CSS), client.css(POPUP_CLOSE_CSS)]

    try:
        client.soft_click(client.await_css(OPT_CSS, is_displayed=True), popups=popups)
    except NoSuchElementException:
        pytest.xfail("Site may have shown a captcha; please run this test again")
        return

    slider = client.await_css(SLIDER_CSS, is_displayed=True)
    time.sleep(2)
    client.scroll_into_view(slider)
    try:
        client.click(slider, popups=popups)
    except ElementClickInterceptedException:
        return False

    return True


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await slider_is_clickable(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await slider_is_clickable(client)
