import pytest

URL = "https://idp5.civis.bz.it/Shibboleth.sso/cielogin?SAMLDS=1&target=https%3A%2F%2Fidp5.civis.bz.it%2Fidp%2FAuthn%2FExternal%3Fconversation%3De2s1&entityID=https%3A%2F%2Fidserver.servizicie.interno.gov.it%2Fidp%2Fprofile%2FSAML2%2FPOST%2FSSO&authnContextClassRef=https%3A%2F%2Fwww.spid.gov.it%2FSpidL2&authnContextComparison=minimum&forceAuthn=true"

BUTTON_CSS = "a.btn-primary.btn-card"
SUPPORTED_CSS = "a[href='https://play.google.com/store/apps/details?id=it.ipzs.cieid']"
UNSUPPORTED_CSS = "#mobile_noChrome"


async def click_button(client):
    await client.navigate(URL, wait="none")
    client.await_css(BUTTON_CSS, is_displayed=True).click()


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await click_button(client)
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await click_button(client)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS, is_displayed=True)
