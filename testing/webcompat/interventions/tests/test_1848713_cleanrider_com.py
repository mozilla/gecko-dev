import pytest
from webdriver import NoSuchElementException
from webdriver.error import (
    ElementClickInterceptedException,
    UnexpectedAlertOpenException,
)

URL = "https://www.cleanrider.com/catalogue/velo-electrique/velos-pliants-electriques/"

COOKIES_CSS = "#didomi-notice-agree-button"
PRICE_SECTION_CSS = "#block-prix"
MIN_THUMB_CSS = "#block-prix .range-min-bullet"


async def can_interact_with_slider(client):
    await client.navigate(URL)

    try:
        client.await_css(COOKIES_CSS, is_displayed=True, timeout=3).click()
    except NoSuchElementException:
        pass

    # there are two copies of the site's markup, one for mobile and one for
    # desktop, with one of them hidden. we have to get the visible one.
    price = client.await_css(PRICE_SECTION_CSS)
    if not client.is_displayed(price):
        client.execute_script("arguments[0].remove()", price)

    min_thumb = client.await_css(MIN_THUMB_CSS)
    mobile_filter = client.find_css('[\\@click="openMobile = true"]')
    if mobile_filter and client.is_displayed(mobile_filter):
        client.scroll_into_view(mobile_filter)
        mobile_filter.click()
        # wait for the button to slide onscreen
        client.execute_async_script(
            """
            var [minThumb, done] = arguments;
            const i = window.setInterval(() => {
                if (minThumb.getBoundingClientRect().x >= 0) {
                    clearInterval(i);
                    done();
                }
            }, 100);
        """,
            min_thumb,
        )

    # we can detect the broken case as the element will not receive mouse events.
    client.execute_script(
        """
        document.documentElement.addEventListener(
          "mousedown",
          () => { alert("bad") },
          true
        );
    """
    )

    client.scroll_into_view(min_thumb)
    try:
        min_thumb.click()
        client.await_css("wait for exception", timeout=5)
    except ElementClickInterceptedException:
        return True
    except UnexpectedAlertOpenException:
        return False


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await can_interact_with_slider(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await can_interact_with_slider(client)
