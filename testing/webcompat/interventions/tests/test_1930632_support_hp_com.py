import pytest
from webdriver.error import NoSuchElementException

URL = "https://support.hp.com/us-en/drivers/hp-pavilion-570-p000-desktop-pc-series/model/15831614?sku=Z5L93AA&serialnumber=CNV7140LDC"

LOADING_CSS = ".loader"
OS_CSS = ".select-os.deadend"
BANNER_CSS = "#driversPageMastiffBannerTop"
CONTACT_CSS = ".contact-support"
VIDEOS_CSS = ".videos-wrapper"
PAGINATION_CSS = ".pagination-container"


async def is_all_content_loading(client):
    await client.navigate(URL, wait="none")
    client.await_css(LOADING_CSS, is_displayed=True)
    client.await_element_hidden(client.css(LOADING_CSS))
    try:
        client.await_css(OS_CSS, is_displayed=True)
        client.await_css(BANNER_CSS, is_displayed=True)
        client.await_css(CONTACT_CSS, is_displayed=True)
        client.await_css(PAGINATION_CSS, is_displayed=True)
        return True
    except NoSuchElementException:
        return False


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_all_content_loading(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_all_content_loading(client)
