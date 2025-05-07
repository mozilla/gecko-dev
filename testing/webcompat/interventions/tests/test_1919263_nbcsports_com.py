import pytest

URL = "https://www.nbcsports.com/nfl/profootballtalk/rumor-mill"
VIDEO_CSS = "video.jw-video"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    client.await_css(VIDEO_CSS, is_displayed=True)


# a test with interventions disabled isn't really possible, as the problem is too intermittent.
