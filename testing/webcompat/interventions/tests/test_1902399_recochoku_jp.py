import pytest

URL = "https://recochoku.jp/"
HERO_CSS = "input.js-search-form"
VPN_TEXT = "This site is limited to access from within Japan"
UNSUPPORTED_TEXT = "Chromeブラウザの最新版をご利用ください"


async def visit_site(client):
    await client.navigate(URL)
    hero, vpn = client.await_first_element_of(
        [client.css(HERO_CSS), client.text(VPN_TEXT)], is_displayed=True
    )
    if vpn:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to Japan.")
        return False
    return True


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    if await visit_site(client):
        assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    if await visit_site(client):
        assert client.find_text(UNSUPPORTED_TEXT, is_displayed=True)
