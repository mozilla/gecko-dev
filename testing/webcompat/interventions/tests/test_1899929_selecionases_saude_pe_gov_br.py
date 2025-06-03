import pytest
from webdriver.error import NoSuchElementException

URL = "https://selecionases.saude.pe.gov.br/SelecionaSES/login"

SUPPORTED_CSS = "#login"
UNSUPPORTED_CSS = "img[alt='Google Chrome']"

# This site seems to stall indefinitely when connecting via VPN on a
# desktop release in automated tests, but not on mobile, and not when
# manually testing, curiously enough.
VPN_MESSAGE = "Please try again using a VPN set to Brazil, or test this site manually on this platform."


async def do_check(client, whichCSS, whichNotCSS):
    try:
        await client.navigate(URL, wait="none")
        client.await_css(whichCSS, timeout=10, is_displayed=True)
        assert not client.find_css(whichNotCSS, is_displayed=True)
    except (ConnectionRefusedError, NoSuchElementException):
        pytest.skip(VPN_MESSAGE)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await do_check(client, SUPPORTED_CSS, UNSUPPORTED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await do_check(client, UNSUPPORTED_CSS, SUPPORTED_CSS)
