import pytest

URL = "https://news.yahoo.co.jp/live"

ARTICLE_LINK_CSS = "a[href*='news.yahoo.co.jp/articles']:has(svg)"
IFRAME_CSS = "iframe[src*=player]"
SUPPORTED_CSS = "video[playsinline]"
UNSUPPORTED_TEXT = "ご利用の環境では映像を視聴できません"
NEED_VPN_TEXT1 = "PLAYER_ERR_GEO_RESTRICTED"
NEED_VPN_TEXT2 = "この映像は日本国外からはご視聴いただけません"


async def go(client, should_work):
    await client.navigate(URL, wait="none")
    client.await_css(ARTICLE_LINK_CSS, is_displayed=True).click()
    # when unsupported, the site only shows an "unsupported" banner, but
    # when supported, it may show the player or vpn messages.
    # it may also nest the player in an iframe.
    iframe = client.css(IFRAME_CSS)
    unsupported = client.text(UNSUPPORTED_TEXT)
    supported = client.css(SUPPORTED_CSS)
    vpn1 = client.text(NEED_VPN_TEXT1)
    vpn2 = client.text(NEED_VPN_TEXT2)
    for i in range(2):
        _iframe, _unsupported, _supported, _vpn1, _vpn2 = client.await_first_element_of(
            [iframe, unsupported, supported, vpn1, vpn2],
            is_displayed=True,
        )
        if _iframe:
            client.switch_frame(_iframe)
            continue
        if should_work:
            assert _supported or _vpn1 or _vpn2
            assert not client.find_element(unsupported)
            return
        assert _unsupported
        assert not client.find_element(supported)
        assert not client.find_element(vpn1)
        assert not client.find_element(vpn2)


@pytest.mark.only_platforms("android", "linux")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await go(client, True)


@pytest.mark.only_platforms("android", "linux")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await go(client, False)
