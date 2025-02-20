import pytest

URL = "https://add.org/adhd-facts/"

MENU_CSS = "nav.mobile-menu-container.mobile-effect"


async def is_menu_visible(client):
    await client.navigate(URL)
    menu = client.await_css(MENU_CSS)
    return client.execute_script(
        """
      return arguments[0].getBoundingClientRect().x < window.innerWidth
      """,
        menu,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_menu_visible(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_menu_visible(client)
