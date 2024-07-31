import pytest

URL = "https://trade-in.vodafone.com/diagnostics/ie/index.html"
SUPPORTED_DESKTOP_ERR = "Cannot run DTL on non mobile browsers"
SUPPORTED_MOBILE_ERR = "Auth token does not exist in memory"
UNSUPPORTED_TEXT = "switch to Chrome"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, platform):
    expected = SUPPORTED_DESKTOP_ERR
    if platform == "android":
        expected = SUPPORTED_MOBILE_ERR
    await client.navigate(URL, await_console_message=expected)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_text(UNSUPPORTED_TEXT)
