import pytest

URL = "https://app.bookcreator.com/sign-in"

SIGN_IN_CSS = ".sign-in-button"
UNSUPPORTED_TEXT = "Firefox is not an officially supported browser"


# We can't test on the android emulator as phone-
# sized screens are not supported by the app.


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(SIGN_IN_CSS, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    client.await_css(SIGN_IN_CSS, is_displayed=True)
    assert client.find_text(UNSUPPORTED_TEXT)
