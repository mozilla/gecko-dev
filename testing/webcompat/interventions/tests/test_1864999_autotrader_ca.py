import time

import pytest
from webdriver import NoSuchElementException, WebDriverException

URL = "https://www.autotrader.ca/cars/bc/chilliwack/?rcp=20&rcs=0&srt=4&prx=250&prv=British+Columbia&loc=v2p5p4&kwd=awd%2B-westminster%2B-richmond&fuel=Electric&hprc=True&wcp=False&sts=New&adtype=Dealer&showcpo=1&inMarket=advancedSearch"

COOKIES_CSS = "#cookie-banner"
FILTER_BUTTON_CSS = "#iconFilter[data-was-processed]"
MAKE_FILTER_CSS = "#faceted-parent-Make"
MAKE_FILTER_OPTS_CSS = "#faceted-parent-Make ul"


async def are_filters_on_right(client):
    await client.navigate(URL)

    try:
        client.remove_element(client.await_css(COOKIES_CSS, timeout=4))
    except NoSuchElementException:
        pass

    # try clicking the button a few times until it hides itself
    btn = client.await_css(FILTER_BUTTON_CSS, is_displayed=True)
    for i in range(5):
        try:
            btn.click()
            time.sleep(1)
        except WebDriverException:
            break

    # scroll to one of the filters and "open" it
    flt = client.await_css(MAKE_FILTER_CSS, is_displayed=True)
    client.scroll_into_view(flt)
    flt.click()

    # check whether the list is offset to the right
    opts = client.await_css(MAKE_FILTER_OPTS_CSS, is_displayed=True)
    return client.execute_script(
        """
      return arguments[0].getBoundingClientRect().x > 0;
    """,
        opts,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await are_filters_on_right(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await are_filters_on_right(client)
