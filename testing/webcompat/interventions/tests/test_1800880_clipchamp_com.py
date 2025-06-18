import pytest

URL = "http://clipchamp.com"
SUPPORTED_TEXT = "Continue with Microsoft"
UNSUPPORTED_CSS = "a[href*='google.com/chrome']"


async def visit_site(client):
    await client.navigate(URL, wait="none")
    client.await_css(
        "a",
        condition="elem.innerText.includes('Try for free')",
        is_displayed=True,
    ).click()


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await visit_site(client)
    assert client.await_text(SUPPORTED_TEXT, is_displayed=True)
    assert not client.find_css(UNSUPPORTED_CSS, is_displayed=True)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await visit_site(client)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
    assert not client.find_text(SUPPORTED_TEXT, is_displayed=True)
