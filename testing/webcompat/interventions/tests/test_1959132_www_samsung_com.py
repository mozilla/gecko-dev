import pytest

URL = "https://www.samsung.com/sec/memory-storage/all-memory-storage/?ssd"
COLUMN1_CSS = ".list-product > .list > :nth-child(1)"
COLUMN2_CSS = ".list-product > .list > :nth-child(2)"


async def shows_two_columns(client):
    await client.navigate(URL, wait="none")
    col1 = client.await_css(COLUMN1_CSS, is_displayed=True)
    col2 = client.await_css(COLUMN2_CSS, is_displayed=True)
    return client.execute_script(
        """
        const [col1, col2] = arguments;
        return col1.getBoundingClientRect().top === col2.getBoundingClientRect().top;
    """,
        col1,
        col2,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await shows_two_columns(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await shows_two_columns(client)
