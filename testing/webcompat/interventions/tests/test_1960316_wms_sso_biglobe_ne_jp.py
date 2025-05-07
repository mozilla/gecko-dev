import pytest

URL = "https://wms.sso.biglobe.ne.jp/webmail/index-tui.jsp?v=0.30-0"

SUPPORTED_URL = "https://auth.sso.biglobe.ne.jp/mail/"
UNSUPPORTED_URL = "https://wms.sso.biglobe.ne.jp/webmail/notsupport.jsp"


# don't skip android, as we want to ensure they do not begin blocking Firefox there as well.
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    redirect = await client.promise_navigation_begins(url=SUPPORTED_URL, timeout=20)
    await client.navigate(URL)
    assert await redirect


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    redirect = await client.promise_navigation_begins(url=UNSUPPORTED_URL, timeout=20)
    await client.navigate(URL)
    assert await redirect
