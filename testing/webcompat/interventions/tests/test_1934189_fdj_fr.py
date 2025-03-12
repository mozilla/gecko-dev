import pytest
from webdriver.error import NoSuchElementException, UnexpectedAlertOpenException

URL = "https://www.fdj.fr/jeux-illiko/instant-euromillions"
COOKIES_ACCEPT_BUTTON_CSS = """button[title="J'accepte"]"""
PLAY_BUTTON_CSS = """[id="Instant Euromillions-btn-play"]"""
GAME_IFRAME_CSS = "#wofOpeningIframe"
UNSUPPORTED_ALERT_MSG = "Cet appareil ou ce navigateur n’est pas compatible avec ce jeu"
RESTRICTED_CSS = "#wsi-restriction-messages-modal-title"
NEED_VPN_TEXT = "Accès refusé"
LOGIN_CSS = "#wsi-login-credentials-title"


async def start_playing(client):
    await client.navigate(URL)
    client.await_css(COOKIES_ACCEPT_BUTTON_CSS).click()
    client.await_css(PLAY_BUTTON_CSS).click()
    try:
        client.await_text(NEED_VPN_TEXT, is_displayed=True, timeout=4)
        pytest.skip("Region-locked, cannot test. Try using a VPN set to France.")
    except NoSuchElementException:
        pass


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await start_playing(client)
    client.switch_frame(client.await_css(GAME_IFRAME_CSS))
    denied, login = client.await_first_element_of(
        [client.css(RESTRICTED_CSS), client.css(LOGIN_CSS)], is_displayed=True
    )
    assert denied or login


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    try:
        await start_playing(client)
    except UnexpectedAlertOpenException as e:
        assert UNSUPPORTED_ALERT_MSG in str(e)
