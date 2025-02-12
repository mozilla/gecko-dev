import pytest

URL = "https://www.fdj.fr/jeux-illiko/instant-euromillions"
COOKIES_ACCEPT_BUTTON_CSS = """button[title="J'accepte"]"""
PLAY_BUTTON_CSS = """[id="Instant Euromillions-btn-play"]"""
GAME_IFRAME_CSS = "#wofOpeningIframe"
UNSUPPORTED_ALERT_CSS = "Cet appareil ou ce navigateur n’est pas compatible avec ce jeu"
RESTRICTED_CSS = "#wsi-restriction-messages-modal-title"
LOGIN_CSS = "#wsi-login-credentials-title"


async def start_playing(client):
    await client.navigate(URL)
    client.await_css(COOKIES_ACCEPT_BUTTON_CSS).click()
    client.await_css(PLAY_BUTTON_CSS).click()


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await start_playing(client)
    client.switch_frame(client.await_css(GAME_IFRAME_CSS))
    denied, login = client.await_first_element_of(
        [client.css(RESTRICTED_CSS), client.css(LOGIN_CSS)], is_displayed=True
    )
    assert denied or login


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    console_message = await client.promise_console_message_listener(
        UNSUPPORTED_ALERT_CSS
    )
    await start_playing(client)
    assert await console_message
