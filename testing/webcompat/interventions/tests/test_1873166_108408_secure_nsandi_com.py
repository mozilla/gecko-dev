import pytest

URL = "https://secure.nsandi.com/thc/policyenforcer/pages/loginB2C.jsf?sec_token=1LF0-PP4E-GVWV-J8DB-1QOR-O84E-W0GD-GIGC"

CHAT_BUTTON_CSS = "#GCLauncherbtn"
UNSUPPORTED_CSS1 = "#unsupported-browser"
UNSUPPORTED_CSS2 = ".browser-warning-fade"


async def is_warning_shown(client):
    await client.navigate(URL)
    client.await_css(CHAT_BUTTON_CSS, is_displayed=True)
    client.await_css(UNSUPPORTED_CSS1)
    client.await_css(UNSUPPORTED_CSS2)
    return client.find_css(UNSUPPORTED_CSS1, is_displayed=True) or client.find_css(
        UNSUPPORTED_CSS2, is_displayed=True
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_warning_shown(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_warning_shown(client)
