import pytest

URL = "https://www.livescience.com/"
TEXT_TO_TEST = ".trending__link"


async def is_text_visible(client):
    # note that the page does not always properly load, so we
    # start loading and wait for the element we want to appear.
    await client.navigate(URL, wait="none")
    link = client.await_css(TEXT_TO_TEST, timeout=10)
    assert client.is_displayed(link)
    return client.execute_async_script(
        """
        const link = arguments[0];
        const cb = arguments[1];
        const fullHeight = link.scrollHeight;
        const parentVisibleHeight = link.parentElement.clientHeight;
        link.style.paddingBottom = "0";
        window.requestAnimationFrame(() => {
            const bottomPaddingHeight = fullHeight - link.scrollHeight;
            cb(fullHeight - parentVisibleHeight <= bottomPaddingHeight);
        });
    """,
        link,
    )


@pytest.mark.only_platforms("windows")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await is_text_visible(client)


@pytest.mark.only_platforms("windows")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await is_text_visible(client)
