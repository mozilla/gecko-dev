import pytest

URL = "https://developers.pinterest.com/docs/web-features/add-ons-overview/"
FIRST_LIST_ITEM_CSS = "#mdx-doc li"


async def list_bullet_point_wraps(client):
    await client.navigate(URL)
    list_item = client.await_css(FIRST_LIST_ITEM_CSS)
    return client.execute_script(
        """
        const li = arguments[0];
        const li_text = li.firstElementChild;
        return li.getBoundingClientRect().height > li_text.getBoundingClientRect().height;
    """,
        list_item,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await list_bullet_point_wraps(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await list_bullet_point_wraps(client)
