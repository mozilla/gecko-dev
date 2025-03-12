import pytest

URL = "https://alidocs.dingtalk.com/not-supported-env?goto=https%3A%2F%2Fdocs.dingtalk.com%2F"

SUPPORTED_URL = "docs.dingtalk.com"
UNSUPPORTED_CSS = "#not-supported-title"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    redirect = await client.promise_navigation_begins(url=SUPPORTED_URL)
    await client.navigate(URL)
    assert await redirect


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
