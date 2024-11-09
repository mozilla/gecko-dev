import pytest

URL = "https://eportal.uestc.edu.cn/new/portal/browerTips.html"

SUPPORTED_CSS = "input[id=username], #ampLoginArticle"
UNSUPPORTED_CSS = "#norSupportBrowser"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
