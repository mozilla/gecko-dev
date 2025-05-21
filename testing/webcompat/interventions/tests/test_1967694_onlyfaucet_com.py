import pytest

URL = "https://onlyfaucet.com/"
BLOCKED_CSS = "#cf-error-details"
UNBLOCKED_CSS = "#content"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(UNBLOCKED_CSS, is_displayed=True)
    assert not client.find_css(BLOCKED_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(BLOCKED_CSS, is_displayed=True)
    assert not client.find_css(UNBLOCKED_CSS, is_displayed=True)
