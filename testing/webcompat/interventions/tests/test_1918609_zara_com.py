import pytest

URL = "https://www.zara.com/us/"

MENU_BUTTON_CSS = "[data-qa-id=layout-header-toggle-menu]"
LOGO_CONTAINER_CSS = "header > *:has(svg.layout-catalog-logo-icon)"


async def are_items_aligned(client):
    await client.navigate(URL)
    menu = client.await_css(MENU_BUTTON_CSS, is_displayed=True)
    logo = client.await_css(LOGO_CONTAINER_CSS, is_displayed=True)
    return client.execute_script(
        """
      const [menu, logo] = arguments;
      return menu.getBoundingClientRect().top === logo.getBoundingClientRect().top;
    """,
        menu,
        logo,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.only_channels("nightly")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await are_items_aligned(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.only_channels("nightly")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await are_items_aligned(client)
