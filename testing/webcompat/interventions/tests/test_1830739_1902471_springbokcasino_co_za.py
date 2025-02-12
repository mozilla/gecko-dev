import pytest

URL = "https://lobby.springbokcasino.co.za:2572/lobby.aspx?skinid=2&springbok=2"
FIRST_GAME_CSS = ".home-view-games-row .gametile"
FIRST_GAME_PRACTICE_CSS = (
    ".detailDialog button:not(#dialogTitleBarCloseBtn).bg-buttonSecondary"
)
UNSUPPORTED_CSS = ".unsupported-device-box"
SUPPORTED_CSS = "#game_main"


async def get_to_page(client):
    await client.navigate(URL)
    client.click(client.await_css(FIRST_GAME_CSS, is_displayed=True))
    client.soft_click(client.await_css(FIRST_GAME_PRACTICE_CSS, is_displayed=True))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await get_to_page(client)
    assert client.await_css(SUPPORTED_CSS, timeout=30)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await get_to_page(client)
    assert client.await_css(UNSUPPORTED_CSS, timeout=30)
