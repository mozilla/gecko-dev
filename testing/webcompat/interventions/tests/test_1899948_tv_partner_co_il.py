from asyncio.exceptions import TimeoutError

import pytest

URL = "https://tv.partner.co.il/#/login"
SUPPORTED_CSS = "[class*=loginForm]"
UNSUPPORTED_CSS = "[class*=bannerBrowser]"


async def try_visit_site(client):
    try:
        await client.navigate(URL, no_skip=True, timeout=30)
    except TimeoutError:
        pytest.skip("Could not connect to site. Try using a VPN set to Israel.")


@pytest.mark.asyncio
@pytest.mark.with_interventions
@pytest.mark.skip_platforms("android")
async def test_enabled(client):
    await try_visit_site(client)
    assert client.await_css(SUPPORTED_CSS)
    assert not client.find_css(UNSUPPORTED_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
@pytest.mark.skip_platforms("android")
async def test_disabled(client):
    await try_visit_site(client)
    assert client.await_css(UNSUPPORTED_CSS)
    assert not client.find_css(SUPPORTED_CSS)
