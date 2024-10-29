import pytest

URL = "https://www.yebocasino.co.za/webplay/"
PRACTICE_CSS = "#lobbybox_featuredgames .gamebox .cta.practice"
IFRAME_CSS = "#gameiframe"
UNSUPPORTED_CSS = ".unsupported-device-box"
SUPPORTED_CSS = "#game_main"


async def get_to_page(client):
    await client.navigate(URL)
    if client.execute_script("return location.href.includes('unavailable')"):
        pytest.skip("Region-locked, cannot test. Try using a VPN set to South Africa.")
        return
    client.soft_click(client.await_css(PRACTICE_CSS))
    client.switch_to_frame(client.await_css(IFRAME_CSS))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await get_to_page(client)
    assert client.await_css(SUPPORTED_CSS, timeout=60)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await get_to_page(client)
    assert client.await_css(UNSUPPORTED_CSS, timeout=60)
