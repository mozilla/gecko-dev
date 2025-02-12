import pytest
from webdriver.error import NoSuchElementException

URL = "https://cfspart-idp.impots.gouv.fr/oauth2/authorize"
HERO_CSS = "#banniereSmartTexte"
OBSOLETE_CSS = "#obsolete.erreur.alert"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(HERO_CSS, is_displayed=True)
    try:
        assert not client.await_css(OBSOLETE_CSS, timeout=5, is_displayed=True)
    except NoSuchElementException:
        assert True


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(HERO_CSS, is_displayed=True)
    assert client.await_css(OBSOLETE_CSS, is_displayed=True)
