import pytest

URL = "https://www.coupangplay.com/"
LOGIN_CSS = "a[href*=login]"
UNSUPPORTED_TEXT = "서비스 이용에 불편을 드려 죄송합니다"
VPN_TEXT = "not available in your region"


async def check_can_login(client):
    await client.navigate(URL)
    login, unsupported, need_vpn = client.await_first_element_of(
        [client.css(LOGIN_CSS), client.text(UNSUPPORTED_TEXT), client.text(VPN_TEXT)],
        is_displayed=True,
    )
    if need_vpn:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to South Korea.")
        return False
    return login


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_can_login(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_can_login(client)
