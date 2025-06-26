import pytest

URL = "https://cdk.fairgoau.com:3572/Lobby.aspx?cdkModule=game&skinId=1&sessionGUID=00000000-0000-0000-0000-000000000000&gameId=18&machId=232&moduleName=plentifultreasure&sessionId=C83FC9F840F3A8C0&pid=10623177&language=EN&user=966995A905B8FF8765&sPassword=&encrypted=True&token=9D32886D-8755-4AFF-91BC-B1024E79890Fs%40Lt&forReal=True&returnURL=https%3A%2F%2Fcdk.fairgoau.com%2Flobby%2F%3FskinId%3D1&integration=external&clientIP=101.176.154.209%0D%0A%20%20%0D%0A%20%20&gameName=Plentiful%20Treasure&target=_self&thirdpartylogin=&isLobbyCore=true&isThemeDark=true&platformUrlHostSet=1"
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
