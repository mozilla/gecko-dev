import pytest
from webdriver import NoSuchElementException

URL = "https://www.cartlow.com/uae/en/search/test"

LOADER_CSS = ".loader.hidden"
MOBILE_FILTER_BUTTON_CSS = "#filterBtn"
LABEL_CSS = "#condition-2 + label"
CHECKED_CSS = "#condition-2:checked"
NOT_CHECKED_CSS = "#condition-2:not(:checked)"


async def filters_work(client):
    await client.navigate(URL, wait="none")

    # remove the loader so clicks don't get intercepted
    client.remove_element(client.await_css(LOADER_CSS))

    # on Android they hide their filters behind a button, so click it first
    mobile_button = client.find_css(MOBILE_FILTER_BUTTON_CSS, is_displayed=True)
    if mobile_button:
        mobile_button.click()

    # click the button twice, and confirm the checkbox toggles on and off
    client.click(client.await_css(LABEL_CSS, is_displayed=True))
    try:
        client.await_css(CHECKED_CSS, timeout=4)
        client.click(client.await_css(LABEL_CSS, is_displayed=True, timeout=4))
        client.await_css(NOT_CHECKED_CSS, timeout=4)
        return True
    except NoSuchElementException:
        return False


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await filters_work(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await filters_work(client)
