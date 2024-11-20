import pytest

URL = "https://la-vache-noire.com/"

COOKIES_CSS = "#cookie-bar"


async def is_cookie_banner_visible(client):
    await client.navigate(URL, wait="none")
    cookies = client.await_css(COOKIES_CSS, is_displayed=True)
    return client.execute_script(
        """
        const b = arguments[0].getBoundingClientRect();
        return window.innerHeight - b.y == b.height;
    """,
        cookies,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_cookie_banner_visible(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_cookie_banner_visible(client)
