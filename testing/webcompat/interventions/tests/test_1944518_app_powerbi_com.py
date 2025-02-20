import pytest
from webdriver import NoSuchElementException

URL = "https://app.powerbi.com/view?r=eyJrIjoiYmVjMjViZjgtYTI2NS00NzcxLWFiMDQtYjM5OGI2YWQzMDUwIiwidCI6IjA0MmQ3NzA5LWMwNWItNGRlZC1hYjg4LTc0NDMwYzU0YmZlNyJ9"

SCROLLBAR_CSS = ".scroll-element"


async def is_scrollbar_added(client):
    await client.navigate(URL)
    try:
        assert client.await_css(SCROLLBAR_CSS, timeout=3)
        return True
    except NoSuchElementException:
        return False


@pytest.mark.only_platforms("mac")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_scrollbar_added(client)


@pytest.mark.only_platforms("mac")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_scrollbar_added(client)
