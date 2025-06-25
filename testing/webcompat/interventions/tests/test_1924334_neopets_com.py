import pytest

URL = "https://www.neopets.com/explore.phtml"
HERO_CSS = "body > span[style*='left: -1000px;']"


async def is_screen_too_wide(client):
    await client.navigate(URL, wait="none")
    assert client.await_css(HERO_CSS)
    return client.execute_script(
        """
      return document.documentElement.scrollWidth > window.innerWidth;
	    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_screen_too_wide(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_screen_too_wide(client)
