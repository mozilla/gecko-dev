import pytest

URL = "https://lovers.cacaushow.com.br/home"

LOGIN_BUTTON_CSS = "button#entrar"
LOGIN_POPUP_CSS = "lovers-login-popup .popup"


async def is_popup_offscreen(client):
    await client.navigate(URL)
    client.soft_click(client.await_css(LOGIN_BUTTON_CSS, is_displayed=True))
    popup = client.await_css(LOGIN_POPUP_CSS, is_displayed=True)
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().top < 0;
      """,
        popup,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_popup_offscreen(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_popup_offscreen(client)
