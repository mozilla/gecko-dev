import pytest

URL = "https://plus.nhk.jp/"

VIDEO_LINK_CSS = "a[href*='plus.nhk.jp/watch']"
VPN_TEXT = "This webpage is not available in your area"
BAD_CSS = "#for_firefox"
GOOD_CSS = "video.hls-player_video, .player--cover--message--button"
UNSUPPORTED_TEXT = "お使いのブラウザ（Firefox）ではご視聴になれません"
UNSUPPORTED2_TEXT = "お使いの環境ではご視聴になれません"


async def saw_firefox_warning(client):
    await client.navigate(URL, wait="none")
    video_link, need_vpn = client.await_first_element_of(
        [client.css(VIDEO_LINK_CSS), client.text(VPN_TEXT)],
        is_displayed=True,
    )
    if need_vpn:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to Japan.")
        return
    found = (
        client.find_css(BAD_CSS, is_displayed=True)
        or client.find_text(UNSUPPORTED_TEXT, is_displayed=True)
        or client.find_text(UNSUPPORTED2_TEXT, is_displayed=True)
    )
    video_link.click()
    return found


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await saw_firefox_warning(client)
    return client.await_css(GOOD_CSS, timeout=30, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_text(UNSUPPORTED2_TEXT, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await saw_firefox_warning(client)
    bad1, bad2 = client.await_first_element_of(
        [client.text(UNSUPPORTED_TEXT), client.text(UNSUPPORTED2_TEXT)], timeout=30
    )
    assert bad1 or bad2
