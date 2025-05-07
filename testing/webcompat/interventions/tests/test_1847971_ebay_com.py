import pytest

URL = "https://www.ebay.com/sl/prelist/suggest"

KEYBOARD_HERO_BUTTON_CSS = ".keyword-suggestion button:has(svg)"
SCANNER_BUTTON_CSS = ".keyword-suggestion button[aria-label='Scan barcode']"
SCANNER_POPUP_CHECK_CSS = ".barcodeReader video"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(SCANNER_BUTTON_CSS, is_displayed=True).click()
    client.await_css(SCANNER_POPUP_CHECK_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_css(KEYBOARD_HERO_BUTTON_CSS, is_displayed=True)
    assert not client.find_css(SCANNER_BUTTON_CSS, is_displayed=True)
