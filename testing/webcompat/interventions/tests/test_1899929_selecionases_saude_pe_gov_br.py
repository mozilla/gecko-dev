import pytest
from webdriver.error import NoSuchElementException

URL = "https://selecionases.saude.pe.gov.br/SelecionaSES/login"

SUPPORTED_CSS = "#login"
UNSUPPORTED_CSS = "img[alt='Google Chrome']"

# This site seems to stall indefinitely when connecting via VPN on a
# desktop release in automated tests, but not on mobile, and not when
# manually testing, curiously enough.
VPN_MESSAGE = "Please try again using a VPN set to Brazil, or test this site manually on this platform."


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="none")
    try:
        client.await_css(SUPPORTED_CSS, timeout=10, is_displayed=True)
        assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)
    except NoSuchElementException:
        pytest.skip(VPN_MESSAGE)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="none")
    try:
        client.await_css(UNSUPPORTED_CSS, timeout=10, is_displayed=True)
        assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
    except NoSuchElementException:
        pytest.skip(VPN_MESSAGE)
