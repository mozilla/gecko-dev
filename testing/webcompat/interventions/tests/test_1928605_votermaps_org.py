import pytest

URL = "https://votermaps.org/"

FIRST_SVG_TEXT = ".map-box svg a path+text"


async def svg_text_is_buggy(client):
    await client.navigate(URL, wait="none")
    text = client.await_css(FIRST_SVG_TEXT, is_displayed=True)

    def get_width():
        return client.execute_script(
            "return arguments[0].getBoundingClientRect().width", text
        )

    orig_width = get_width()
    client.execute_script(
        """
       const text = arguments[0].childNodes[0];
       text.nodeValue = text.nodeValue.trim();
    """,
        text,
    )
    return orig_width > get_width() + 10


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await svg_text_is_buggy(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await svg_text_is_buggy(client)
