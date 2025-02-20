import pytest

URL = "https://watch.foxtel.com.au/app/#/live/fs1"
SUPPORTED_CSS = ".login-page"
UNSUPPORTED_CSS = ".ua-barrier"
NEED_VPN_TEXT = "Access Denied"


async def visit_site(client, expected):
    await client.navigate(URL)
    expected, vpn = client.await_first_element_of(
        [client.css(expected), client.text(NEED_VPN_TEXT)], is_displayed=True
    )
    if vpn:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to Australia.")
    return expected


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await visit_site(client, SUPPORTED_CSS)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await visit_site(client, UNSUPPORTED_CSS)
    assert not client.find_css(SUPPORTED_CSS)
