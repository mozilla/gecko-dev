import pytest
from webdriver.error import NoSuchElementException, UnexpectedAlertOpenException

URL = "https://www.fdj.fr/jeux-illiko/instant-euromillions"
COOKIES_ACCEPT_BUTTON_CSS = """button[title="J'accepte"]"""
PLAY_BUTTON_CSS = """[id="Instant Euromillions-btn-play"]"""
UNSUPPORTED_ALERT_MSG = "Cet appareil ou ce navigateur n’est pas compatible avec ce jeu"
NEED_VPN_TEXT = "Accès refusé"


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
@pytest.mark.without_interventions
async def test_disabled(client):
    saw_msg = False
    try:
        await start_playing(client)
    except UnexpectedAlertOpenException as e:
        if UNSUPPORTED_ALERT_MSG in str(e):
            saw_msg = True
    assert not saw_msg
