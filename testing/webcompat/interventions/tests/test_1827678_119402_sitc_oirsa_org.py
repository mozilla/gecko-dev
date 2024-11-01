import pytest

URL = "http://sitc.oirsa.org/sitc/Login.aspx"

SUPPORTED_CSS = "form"
UNSUPPORTED_CSS = "#divBrowserAlert"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUPPORTED_CSS)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=False)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
