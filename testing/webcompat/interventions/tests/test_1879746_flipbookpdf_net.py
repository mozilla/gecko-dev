import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.flipbookpdf.net/web/site/46b285655a642a5c190abf99e5b21cb4FLIPTEST.pdf.html"

LOADER_CSS = ".loader"
ZOOM_IN_CSS = ".zoom-icon.zoom-icon-in"
ZOOM_OUT_CSS = ".zoom-icon.zoom-icon-out"


async def check_if_can_zoom_out(client):
    await client.navigate(URL)

    # If we click too early, it can break the page.
    client.await_css(LOADER_CSS, is_displayed=True)
    client.await_element_hidden(client.css(LOADER_CSS))

    # Normally, clicking on the zoom-in icon in the control bar of the viewer
    # hides it, and changes it to a zoom-out icon. But when things are broken,
    # the page does zoom in, but the icon never changes.
    client.await_css(ZOOM_IN_CSS, is_displayed=True).click()
    try:
        client.await_css(ZOOM_OUT_CSS, is_displayed=True, timeout=3)
        return True
    except NoSuchElementException:
        return False


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_if_can_zoom_out(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_if_can_zoom_out(client)
