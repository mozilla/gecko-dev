import pytest
from webdriver.error import NoSuchElementException

URL = "https://facturador.afip.gob.ar/#/bienvenida"

HERO_CSS = "#btnInstalar"
UNSUPPORTED_CSS = "img[src*=chrome]"


async def visit_site(client):
    await client.navigate(URL, wait="none")
    try:
        client.await_css(HERO_CSS, is_displayed=True, timeout=90)
        return True
    except NoSuchElementException:
        pytest.skip(
            "Site seems to be loading very slowly. Try using a VPN set to Argentina."
        )
        return False


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await visit_site(client)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await visit_site(client)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=True)
