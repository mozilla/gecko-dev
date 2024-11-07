import pytest

URL = "https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/OpenGLES_ProgrammingGuide/AdoptingOpenGLES3/AdoptingOpenGLES3.html"

TOC_CSS = "#tocContainer"


async def check_toc_fits_on_screen(client):
    await client.navigate(URL)
    toc = client.await_css(TOC_CSS, is_displayed=True)
    return client.execute_script(
        """
        bb = arguments[0].getBoundingClientRect();
        return bb.left >= window.scrollX && bb.left <= window.innerWidth + window.scrollX &&
               bb.top >= window.scrollY && bb.top <= window.innerHeight + window.scrollY
    """,
        toc,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_toc_fits_on_screen(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_toc_fits_on_screen(client)
