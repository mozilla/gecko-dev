import pytest

URL = "https://app.genio.co/"

USERNAME_CSS = "input[type=email]"
PASSWORD_CSS = "input[type=password]"
LOGIN_CSS = "input[type=submit]"
SUPPORTED_CSS = "#homeNavigation"
UNSUPPORTED_CSS = "a[href*='google.com/chrome']"


async def login(client, credentials):
    await client.navigate(URL, wait="none")
    client.await_css(USERNAME_CSS, is_displayed=True).send_keys(credentials["username"])
    client.await_css(PASSWORD_CSS, is_displayed=True).send_keys(credentials["password"])
    client.await_css(LOGIN_CSS, is_displayed=True).click()


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client, credentials):
    await login(client, credentials)
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client, credentials):
    await login(client, credentials)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
