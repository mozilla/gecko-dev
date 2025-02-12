import pytest

URL = "https://www.beterbed.nl/matrassen/m-line?SelectedFacetIds[1dfabd26b0e444e68fc92fd50281f0be]=M%20line&SortBy=DiscountPercentage"
CONTAINER_CSS = ".top-navigation__items.slider-content"


async def is_scrollbar_visible(client):
    await client.navigate(URL)
    container = client.await_css(CONTAINER_CSS)
    return client.execute_script(
        """
      const container = arguments[0];
      return Math.round(container.getBoundingClientRect().height) != container.clientHeight;
    """,
        container,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_scrollbar_visible(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.need_visible_scrollbars
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_scrollbar_visible(client)
