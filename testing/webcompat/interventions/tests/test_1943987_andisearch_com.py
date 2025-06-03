import pytest

URL = "https://andisearch.com"
MSGBAR_CSS = ".rcw-sender"


async def is_msgbar_offscreen(client):
    await client.navigate(URL, wait="none")
    msgbar = client.await_css(MSGBAR_CSS, is_displayed=True)
    return client.execute_script(
        """
        const msgbar = arguments[0];
        return msgbar.getBoundingClientRect().bottom > window.innerHeight;
    """,
        msgbar,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.actual_platform_required
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_msgbar_offscreen(client)


@pytest.mark.only_platforms("android")
@pytest.mark.actual_platform_required
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_msgbar_offscreen(client)
