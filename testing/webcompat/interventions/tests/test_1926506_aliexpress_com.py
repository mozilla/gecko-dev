import pytest

URL = "https://www.aliexpress.com/"

SUPER_DEALS_LINK_CSS = "[data-spm=superdeal] a[href*=ssr]"
CONTAINER_CSS = ".aec-scrollview-horizontal.tab_pc_content"


async def is_scrollbar_visible(client):
    await client.navigate(URL)
    link = client.await_css(SUPER_DEALS_LINK_CSS)
    await client.navigate(client.get_element_attribute(link, "href"))
    container = client.await_css(CONTAINER_CSS, is_displayed=True)
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
