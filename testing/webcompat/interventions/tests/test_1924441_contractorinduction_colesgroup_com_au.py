import pytest

URL = "https://contractorinduction.colesgroup.com.au/"

CAPTCHA_CSS = "iframe[src*='Incapsula_Resource']"
SUPPORTED_CSS = "#EmailAddress"
UNSUPPORTED_TEXT = "we do not support your current browser"


async def visit_site(client):
    await client.navigate(URL, wait="none")
    _, _, vpn = client.await_first_element_of(
        [
            client.css(SUPPORTED_CSS),
            client.text(UNSUPPORTED_TEXT),
            client.css(CAPTCHA_CSS),
        ],
        is_displayed=True,
    )
    if vpn:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to Australia.")


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await visit_site(client)
    assert client.await_css(SUPPORTED_CSS, is_displayed=True)
    assert not client.find_text(UNSUPPORTED_TEXT)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await visit_site(client)
    assert client.await_text(UNSUPPORTED_TEXT, is_displayed=True)
    assert not client.find_css(SUPPORTED_CSS)
