import pytest

URL = "https://livelesson.class.com/class/8a731bd4-17f2-4bf6-a1b4-4f3561cab2bd"

LOADER_CSS = "#root svg"
JOIN_BUTTON_CSS = "#root button"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(JOIN_BUTTON_CSS)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(LOADER_CSS)
    assert client.execute_async_script(
        """
        done = arguments[0];
        setInterval(() => {
            if (!document.documentElement.innerText) {
                done(true);
            }
        }, 100);
    """
    )
