import pytest

URL = "https://www.livescience.com/"
TEXT_TO_TEST = ".trending__link"


async def is_text_visible(client):
    await client.navigate(URL)
    link = client.await_css(TEXT_TO_TEST)
    assert client.is_displayed(link)
    return client.execute_script(
        """
        const link = arguments[0];
        return link.parentElement.clientHeight > link.scrollHeight;
""",
        link,
    )


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.with_interventions
@pytest.mark.no_overlay_scrollbars
async def test_enabled(client):
    assert await is_text_visible(client)


@pytest.mark.skip_platforms("android", "mac")
@pytest.mark.asyncio
@pytest.mark.without_interventions
@pytest.mark.no_overlay_scrollbars
async def test_disabled(client):
    assert not await is_text_visible(client)
