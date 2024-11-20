from asyncio.exceptions import TimeoutError

import pytest

URL = "https://www.microsoft.com/en-us"

COOKIES_CSS = "#wcpConsentBannerCtrl"
SEARCH_CSS = "#search"
INPUT_CSS = "#cli_shellHeaderSearchInput"
SUGGESTION_CSS = ".m-auto-suggest a.f-product"


async def does_enter_work(client):
    await client.navigate(URL)

    # Wait for the cookie banner. By then the page's event listeners are set up.
    client.await_css(COOKIES_CSS, is_displayed=True)

    client.await_css(SEARCH_CSS, is_displayed=True).click()
    client.await_css(INPUT_CSS, is_displayed=True).send_keys("surface")

    # We now wait for the search suggestions to appear, press Down twice,
    # and Enter once. This should begin a navigation if things are working.
    nav = await client.promise_navigation_begins(url="microsoft", timeout=2)
    client.await_css(SUGGESTION_CSS, is_displayed=True)
    client.keyboard.key_down("\ue05b").perform()  # Down arrow
    client.keyboard.key_down("\ue05b").perform()  # Down arrow
    client.keyboard.key_down("\ue007").perform()  # Enter
    try:
        await nav
        return True
    except TimeoutError:
        return False


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await does_enter_work(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await does_enter_work(client)
