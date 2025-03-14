import pytest

URL = "https://platform.anecdotes.ai/"

SUPPORTED_CSS = "app-text-field#email"
UNSUPPORTED_URL = "browser-not-supported"


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(SUPPORTED_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    redirect = await client.promise_navigation_begins(url=UNSUPPORTED_URL, timeout=2)
    await client.navigate(URL)
    assert await redirect
